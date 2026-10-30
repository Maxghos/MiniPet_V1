package cl.maxghos.minipet

import kotlinx.coroutines.*
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.json.JSONObject
import java.io.Closeable
import java.net.HttpURLConnection
import java.net.URI
import java.net.URL
import java.util.Base64
import java.util.concurrent.atomic.AtomicInteger

interface CommandTransport : Closeable {
    val chunkSize: Int
    suspend fun exchange(command: JSONObject): JSONObject
}
class DeviceFailure(message: String): Exception(message)
object LocalAddress {
    fun normalize(input: String): String {
        val value = input.trim().let { if (it.startsWith("http://")) it else "http://$it" }
        val uri = URI(value)
        require(uri.scheme == "http" && uri.userInfo == null && uri.query == null && uri.fragment == null && (uri.path.isNullOrEmpty() || uri.path == "/")) { "Introduce solo la IP local de MiniPet" }
        val host = requireNotNull(uri.host) { "Dirección inválida" }
        val parts = host.split('.')
        val localV4 = parts.size == 4 && parts.all { (it.toIntOrNull() ?: -1) in 0..255 } &&
            (parts[0] == "10" || parts[0] == "127" || (parts[0] == "192" && parts[1] == "168") ||
                (parts[0] == "172" && parts[1].toInt() in 16..31) || (parts[0] == "169" && parts[1] == "254"))
        val localName = host.endsWith(".local", true) && host.matches(Regex("[A-Za-z0-9.-]+"))
        val ip6 = host.removePrefix("[").removeSuffix("]").lowercase()
        require(localV4 || localName || (ip6.contains(':') && (ip6.startsWith("fe80:") || ip6.startsWith("fd") || ip6.startsWith("fc")))) { "Usa la dirección local, en la misma Wi-Fi" }
        val port = if (uri.port == -1) 8080 else uri.port
        require(port in 1..65535)
        return "http://$host:$port"
    }
}
class HttpTransport(address: String, private val open: (URL) -> HttpURLConnection = { it.openConnection() as HttpURLConnection }): CommandTransport {
    private val base = LocalAddress.normalize(address)
    override val chunkSize = 3072
    override suspend fun exchange(command: JSONObject): JSONObject = withContext(Dispatchers.IO) {
        val connection = open(URL("$base/api/v1/command"))
        try {
            connection.connectTimeout = 5000; connection.readTimeout = 12000
            connection.requestMethod = "POST"; connection.doOutput = true
            connection.setRequestProperty("Content-Type", "application/json; charset=utf-8")
            if (command.has("token")) connection.setRequestProperty("Authorization", "Bearer ${command.getString("token")}")
            val bytes = command.toString().toByteArray(Charsets.UTF_8)
            require(bytes.size <= 12288)
            connection.setFixedLengthStreamingMode(bytes.size)
            connection.outputStream.use { it.write(bytes) }
            val status = connection.responseCode
            val stream = if (status in 200..299) connection.inputStream else connection.errorStream
            val body = stream?.use { String(it.readBytesLimited(65536), Charsets.UTF_8) } ?: ""
            if (status != 200 || body.length > 65536) throw DeviceFailure("MiniPet no pudo atender la solicitud")
            JSONObject(body)
        } finally { connection.disconnect() }
    }
    override fun close() = Unit
}
class DeviceClient(val transport: CommandTransport, var token: String = ""): Closeable {
    private val mutex = Mutex(); private val sequence = AtomicInteger(1)
    suspend fun call(op: String, fields: JSONObject = JSONObject()): JSONObject = mutex.withLock {
        val request = JSONObject(fields.toString()).put("op", op).put("req", sequence.getAndIncrement())
        if (token.isNotEmpty()) request.put("token", token)
        val response = transport.exchange(request)
        if (!response.optBoolean("ok")) throw DeviceFailure(response.optString("error", "MiniPet rechazó la solicitud"))
        require(response.optInt("req",-1)==request.getInt("req")) { "Respuesta de otra solicitud; vuelve a conectar" }
        return response
    }
    suspend fun pair(code: String): String {
        require(code.matches(Regex("[0-9]{6}"))) { "El PIN tiene seis números" }
        val response = call("pair", JSONObject().put("code", code))
        token = response.getString("token")
        require(token.matches(Regex("[0-9a-f]{64}")))
        return token
    }
    suspend fun upload(id: String, title: String, bytes: ByteArray, progress: (Float) -> Unit) {
        MediaFormat.parse(bytes)
        require(id.matches(Regex("[A-Za-z0-9_-]{1,24}")))
        call("upload.begin", JSONObject().put("id", id).put("title", title).put("bytes", bytes.size))
        try {
            var offset = 0
            while (offset < bytes.size) {
                currentCoroutineContext().ensureActive()
                val end = minOf(bytes.size, offset + transport.chunkSize)
                val encoded = Base64.getEncoder().encodeToString(bytes.copyOfRange(offset,end))
                val response = call("upload.chunk", JSONObject().put("id", id).put("offset", offset).put("data", encoded))
                require(response.getInt("offset") == end) { "Transferencia incompleta; vuelve a intentarlo" }
                offset = end; progress(offset.toFloat() / bytes.size)
            }
            call("upload.commit", JSONObject().put("id",id))
        } catch (error: Exception) {
            withContext(NonCancellable) { runCatching { call("upload.cancel") } }
            throw error
        }
    }
    override fun close() = transport.close()
}
