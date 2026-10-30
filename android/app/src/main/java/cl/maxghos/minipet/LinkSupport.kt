package cl.maxghos.minipet

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import java.io.Closeable
import java.net.HttpURLConnection
import java.net.URL
import java.security.KeyStore
import java.security.MessageDigest
import java.util.Base64
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

class LinkVault(context: Context) {
    private val preferences=context.getSharedPreferences("links",Context.MODE_PRIVATE)
    private fun key(): SecretKey {
        val store=KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        (store.getKey("MiniPetLink",null) as? SecretKey)?.let { return it }
        return KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES,"AndroidKeyStore").apply {
            init(KeyGenParameterSpec.Builder("MiniPetLink",KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE).build())
        }.generateKey()
    }
    private fun slot(endpoint: String)=MessageDigest.getInstance("SHA-256").digest(endpoint.toByteArray()).joinToString(""){"%02x".format(it)}
    fun save(endpoint: String,token: String) {
        val cipher=Cipher.getInstance("AES/GCM/NoPadding");cipher.init(Cipher.ENCRYPT_MODE,key())
        val bytes=cipher.iv+cipher.doFinal(token.toByteArray(Charsets.UTF_8))
        val encoded=Base64.getEncoder().encodeToString(bytes)
        preferences.edit().putString(slot(endpoint),encoded).putString(slot("current-device"),encoded).apply()
    }
    fun load(endpoint: String): String = runCatching {
        val saved=preferences.getString(slot("current-device"),null) ?: preferences.getString(slot(endpoint),null) ?: return ""
        val bytes=Base64.getDecoder().decode(saved);require(bytes.size>12)
        val cipher=Cipher.getInstance("AES/GCM/NoPadding");cipher.init(Cipher.DECRYPT_MODE,key(),GCMParameterSpec(128,bytes.copyOfRange(0,12)))
        String(cipher.doFinal(bytes.copyOfRange(12,bytes.size)),Charsets.UTF_8)
    }.getOrDefault("")
    var address: String
        get()=preferences.getString("address","192.168.4.1:8080") ?: "192.168.4.1:8080"
        set(value){preferences.edit().putString("address",value).apply()}
}
class WifiDiscovery(context: Context,private val found: (String,String)->Unit): Closeable {
    private val manager=context.getSystemService(Context.NSD_SERVICE) as NsdManager
    private var started=false;private var resolving=false
    private val pending=ArrayDeque<NsdServiceInfo>()
    private val listener=object:NsdManager.DiscoveryListener {
        override fun onDiscoveryStarted(type: String)=Unit
        override fun onDiscoveryStopped(type: String)=Unit
        override fun onStartDiscoveryFailed(type: String,error: Int){close()}
        override fun onStopDiscoveryFailed(type: String,error: Int)=Unit
        override fun onServiceLost(info: NsdServiceInfo)=Unit
        override fun onServiceFound(info: NsdServiceInfo){synchronized(pending){pending.addLast(info)};resolveNext()}
    }
    @Suppress("DEPRECATION")
    private fun resolveNext(){
        synchronized(pending){
            if(!started||resolving||pending.isEmpty())return
            resolving=true
            val info=pending.removeFirst()
            manager.resolveService(info,object:NsdManager.ResolveListener {
                override fun onResolveFailed(info: NsdServiceInfo,error: Int){resolving=false;resolveNext()}
                override fun onServiceResolved(info: NsdServiceInfo){
                    info.host?.hostAddress?.let { host ->
                        val address=if(host.contains(':'))"[$host]:${info.port}" else "$host:${info.port}"
                        if(started)found(info.serviceName,address)
                    }
                    resolving=false;resolveNext()
                }
            })
        }
    }
    fun start(){if(!started){started=true;manager.discoverServices("_minipet._tcp.",NsdManager.PROTOCOL_DNS_SD,listener)}}
    override fun close(){if(started){started=false;runCatching{manager.stopServiceDiscovery(listener)}}}
}
fun wifiConnection(context: Context,url: URL): HttpURLConnection {
    val manager=context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
    val wifi=manager.allNetworks.firstOrNull { manager.getNetworkCapabilities(it)?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)==true }
    return (wifi?.openConnection(url) ?: url.openConnection()) as HttpURLConnection
}
