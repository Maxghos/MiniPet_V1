package cl.maxghos.minipet

import kotlinx.coroutines.*
import org.json.JSONObject
import java.io.InputStream
import java.io.OutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.security.MessageDigest

data class FirmwarePackage(val target: String,val version: String,val imageBytes: Int,val sha256: String){
    val totalBytes: Long get()=FirmwareBundle.HEADER_BYTES.toLong()+imageBytes
}

/** MPFW wraps an ESP32-S3 application image, not a merged flash/bootloader dump. */
object FirmwareBundle {
    const val HEADER_BYTES=128
    const val MAX_IMAGE_BYTES=6_553_600
    private val targets=setOf("minipet-s3-n16r8-touch","minipet-s3-n16r8-button")
    private fun unsigned(bytes: ByteArray,at: Int)=bytes[at].toInt() and 255
    private fun short(bytes: ByteArray,at: Int)=unsigned(bytes,at) or (unsigned(bytes,at+1) shl 8)
    private fun field(header: ByteArray,at: Int): String {
        val end=(at until at+32).firstOrNull{header[it]==0.toByte()}
            ?: throw IllegalArgumentException("Campo del paquete sin terminación")
        require(end>at&&(at until end).all{unsigned(header,it) in 33..126}&&(end until at+32).all{header[it]==0.toByte()}){"Campo del paquete inválido"}
        return String(header,at,end-at,Charsets.US_ASCII)
    }
    internal fun header(input: InputStream): Pair<ByteArray,FirmwarePackage> {
        val bytes=ByteArray(HEADER_BYTES);readExactly(input,bytes)
        require(String(bytes,0,4,Charsets.US_ASCII)=="MPFW"&&short(bytes,4)==1&&short(bytes,6)==HEADER_BYTES){"Elige un paquete MiniPet .mpfw válido"}
        val size=(0..3).fold(0L){value,index->value or (unsigned(bytes,8+index).toLong() shl (index*8))}
        require(size in 1024..MAX_IMAGE_BYTES.toLong()){"Tamaño de firmware fuera de los límites"}
        val target=field(bytes,12);val version=field(bytes,44)
        require(target in targets){"El paquete no es para MiniPet ESP32-S3 N16R8"}
        require(version.matches(Regex("[A-Za-z0-9][A-Za-z0-9._+\\-]{0,30}"))){"Versión del paquete inválida"}
        require((108 until HEADER_BYTES).all{bytes[it]==0.toByte()}){"Formato de paquete no compatible"}
        return bytes to FirmwarePackage(target,version,size.toInt(),hex(bytes.copyOfRange(76,108)))
    }
    private fun readExactly(input: InputStream,bytes: ByteArray){
        var position=0
        while(position<bytes.size){
            val count=input.read(bytes,position,bytes.size-position)
            require(count>=0){"Paquete de firmware incompleto"}
            if(count==0){val next=input.read();require(next>=0){"Paquete de firmware incompleto"};bytes[position++]=next.toByte()}
            else position+=count
        }
    }
    private fun imageHeader(bytes: ByteArray){
        require(unsigned(bytes,0)==0xe9&&unsigned(bytes,1) in 1..16&&short(bytes,12)==9){"El paquete no contiene una aplicación ESP32-S3 válida"}
    }
    internal fun hex(bytes: ByteArray)=bytes.joinToString(""){(it.toInt() and 255).toString(16).padStart(2,'0')}
    internal suspend fun image(input: InputStream,info: FirmwarePackage,output: OutputStream?=null,progress:(Float)->Unit={}){
        val digest=MessageDigest.getInstance("SHA-256")
        val first=ByteArray(24);readExactly(input,first);imageHeader(first);digest.update(first);output?.write(first)
        val buffer=ByteArray(16*1024);var received=first.size
        while(received<info.imageBytes){
            currentCoroutineContext().ensureActive()
            val count=input.read(buffer,0,minOf(buffer.size,info.imageBytes-received))
            require(count>=0){"Paquete de firmware incompleto"}
            if(count==0){val next=input.read();require(next>=0){"Paquete de firmware incompleto"};buffer[0]=next.toByte();digest.update(buffer,0,1);output?.write(buffer,0,1);received++}
            else{digest.update(buffer,0,count);output?.write(buffer,0,count);received+=count}
            progress(received.toFloat()/info.imageBytes)
        }
        require(input.read()==-1){"El paquete contiene bytes adicionales"}
        require(hex(digest.digest())==info.sha256){"El archivo de firmware está dañado: SHA-256 no coincide"}
        currentCoroutineContext().ensureActive()
    }
    suspend fun inspect(open:()->InputStream): FirmwarePackage=withContext(Dispatchers.IO){
        currentCoroutineContext().ensureActive()
        open().use{input->val (_,info)=header(input);image(input,info);info}
    }
}

internal suspend fun transferFirmware(url: URL,connect:(URL)->HttpURLConnection,token: String,info: FirmwarePackage,code: String,
                                     open:()->InputStream,progress:(Float)->Unit): JSONObject=withContext(Dispatchers.IO){
    require(token.matches(Regex("[0-9a-f]{64}"))){"Vincula la app antes de actualizar"}
    require(code.matches(Regex("[0-9]{6}"))){"Escribe las seis cifras de la OLED"}
    // Recheck before constructing an HTTP connection; a document provider may have changed its file.
    require(FirmwareBundle.inspect(open)==info){"El archivo cambió; vuelve a seleccionarlo"}
    open().use{input->
        val (header,current)=FirmwareBundle.header(input)
        require(current==info){"El archivo cambió; vuelve a seleccionarlo"}
        val connection=connect(url)
        val cancellation=launch(Dispatchers.Default,start=CoroutineStart.UNDISPATCHED){
            try{awaitCancellation()}finally{connection.disconnect()}
        }
        try{
            connection.connectTimeout=5000;connection.readTimeout=12000
            connection.instanceFollowRedirects=false
            connection.requestMethod="POST";connection.doOutput=true
            connection.setRequestProperty("Content-Type","application/octet-stream")
            connection.setRequestProperty("Authorization","Bearer $token")
            connection.setRequestProperty("X-MiniPet-Code",code)
            connection.setFixedLengthStreamingMode(info.totalBytes)
            progress(0f)
            connection.outputStream.use{output->output.write(header);FirmwareBundle.image(input,info,output){progress(it.coerceAtMost(.99f))}}
            currentCoroutineContext().ensureActive()
            val status=connection.responseCode
            val stream=if(status in 200..299)connection.inputStream else connection.errorStream
            val body=stream?.use{String(it.readBytesLimited(4096),Charsets.UTF_8)}?:""
            val reply=runCatching{JSONObject(body)}.getOrNull()
            if(status!=200||reply==null||!reply.optBoolean("ok")||!reply.optBoolean("reboot"))
                throw DeviceFailure(reply?.optString("error")?.takeIf{it.isNotBlank()}?:"MiniPet no confirmó la actualización. Revisa su estado antes de reintentar.")
            progress(1f);reply
        }catch(error:CancellationException){throw error}
        catch(error:DeviceFailure){throw error}
        catch(_:Exception){throw DeviceFailure("La actualización no quedó confirmada. Reconecta y comprueba la versión de MiniPet.")}
        finally{cancellation.cancel();connection.disconnect()}
    }
}
