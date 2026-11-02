package cl.maxghos.minipet

import kotlinx.coroutines.*
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.ServerSocket
import java.net.URL
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.MessageDigest
import java.util.concurrent.atomic.AtomicReference
import kotlin.concurrent.thread

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[28],manifest=Config.NONE)
class FirmwareUpdateTest {
    private fun bundle(imageBytes:Int=1024,target:String="minipet-s3-n16r8-touch",version:String="3.3.0"):ByteArray {
        val image=ByteArray(imageBytes){(it*37).toByte()};image[0]=0xe9.toByte();image[1]=5;image[12]=9;image[13]=0
        val header=ByteArray(128);"MPFW".toByteArray().copyInto(header)
        ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN).putShort(4,1).putShort(6,128).putInt(8,image.size)
        target.toByteArray(Charsets.US_ASCII).copyInto(header,12);version.toByteArray(Charsets.US_ASCII).copyInto(header,44)
        MessageDigest.getInstance("SHA-256").digest(image).copyInto(header,76)
        return header+image
    }
    private suspend fun rejects(bytes:ByteArray){
        try{FirmwareBundle.inspect{ByteArrayInputStream(bytes)};fail("Expected invalid firmware rejection")}
        catch(_:IllegalArgumentException){}
    }
    @Test fun validatesAStreamingPackageAndBothBoardTargets()=runBlocking{
        for(target in listOf("minipet-s3-n16r8-touch","minipet-s3-n16r8-button")){
            val bytes=bundle(target=target)
            val info=FirmwareBundle.inspect{object:ByteArrayInputStream(bytes){override fun read(data:ByteArray,offset:Int,length:Int)=super.read(data,offset,minOf(length,7))}}
            assertEquals(target,info.target);assertEquals("3.3.0",info.version);assertEquals(1024,info.imageBytes);assertEquals(1152L,info.totalBytes)
            assertEquals(FirmwareBundle.hex(MessageDigest.getInstance("SHA-256").digest(bytes.copyOfRange(128,bytes.size))),info.sha256)
        }
    }
    @Test fun rejectsCorruptionTruncationExtraBytesAndBadImage()=runBlocking{
        val bytes=bundle()
        rejects(bytes.copyOf(100));rejects(bytes.copyOf(bytes.size-1));rejects(bytes+byteArrayOf(0))
        rejects(bytes.copyOf().also{it[bytes.lastIndex]=(it.last().toInt() xor 1).toByte()})
        rejects(bytes.copyOf().also{it[128]=0})
        val wrongChip=bytes.copyOf();wrongChip[128+12]=0
        MessageDigest.getInstance("SHA-256").digest(wrongChip.copyOfRange(128,wrongChip.size)).copyInto(wrongChip,76)
        rejects(wrongChip)
    }
    @Test fun rejectsMalformedHeadersBeforeReadingOversizedImages()=runBlocking{
        val bytes=bundle()
        for(offset in listOf(0,4,6,108,127))rejects(bytes.copyOf().also{it[offset]=(it[offset].toInt() xor 1).toByte()})
        rejects(bundle(target="another-board"));rejects(bundle(version="bad version"))
        for(size in listOf(1023L,6_553_601L,0xffffffffL)){
            val header=bytes.copyOf(128);ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN).putInt(8,size.toInt());rejects(header)
        }
        rejects(bytes.copyOf().also{for(i in 12 until 44)it[i]='A'.code.toByte()})
    }
    @Test fun invalidOrChangedPackagesAndBleNeverStartHttp()=runBlocking{
        val bytes=bundle();val info=FirmwareBundle.inspect{bytes.inputStream()};var connected=false
        val client=DeviceClient(HttpTransport("127.0.0.1:8080"){connected=true;throw AssertionError("Must not connect")},"a".repeat(64))
        for(code in listOf("12345","abcdef","1234567")){
            try{client.updateFirmware(info,code,{bytes.inputStream()}){};fail("Expected PIN validation")}
            catch(_:IllegalArgumentException){}
        }
        val changed=bundle(version="3.3.1")
        try{client.updateFirmware(info,"123456",{changed.inputStream()}){};fail("Expected source revalidation")}
        catch(_:IllegalArgumentException){}
        assertFalse(connected)
        val ble=object:CommandTransport {
            override val chunkSize=120
            override suspend fun exchange(command:JSONObject):JSONObject=throw AssertionError("Must not send")
            override fun close()=Unit
        }
        try{DeviceClient(ble,"a".repeat(64)).updateFirmware(info,"123456",{throw AssertionError("Must not read")}){};fail("Expected Wi-Fi requirement")}
        catch(_:DeviceFailure){}
    }
    @Test fun actualHttpCarriesTheExactBundleAndRequiresExplicitRebootAcceptance():Unit=runBlocking{
        val bytes=bundle(32768);val info=FirmwareBundle.inspect{bytes.inputStream()};val failure=AtomicReference<Throwable?>()
        ServerSocket(0).use{server->
            server.soTimeout=10000
            val worker=thread{
                try{server.accept().use{socket->
                    socket.soTimeout=10000;val input=socket.getInputStream();val headers=StringBuilder()
                    while(!headers.endsWith("\r\n\r\n")){val c=input.read();check(c>=0);headers.append(c.toChar())}
                    assertTrue(headers.startsWith("POST /api/v1/firmware HTTP/1.1"))
                    assertTrue(headers.contains("Authorization: Bearer ${"a".repeat(64)}",ignoreCase=true))
                    assertTrue(headers.contains("X-MiniPet-Code: 123456",ignoreCase=true))
                    assertTrue(headers.contains("Content-Type: application/octet-stream",ignoreCase=true))
                    val length=Regex("Content-Length: (\\d+)",RegexOption.IGNORE_CASE).find(headers)!!.groupValues[1].toInt()
                    assertEquals(bytes.size,length);val actual=ByteArray(length);var offset=0
                    while(offset<length){val count=input.read(actual,offset,length-offset);check(count>0);offset+=count}
                    assertArrayEquals(bytes,actual)
                    val body="{\"ok\":true,\"reboot\":true}".toByteArray()
                    socket.getOutputStream().write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ${body.size}\r\nConnection: close\r\n\r\n".toByteArray()+body)
                }}catch(error:Throwable){failure.set(error)}
            }
            val progress=mutableListOf<Float>()
            val response=DeviceClient(HttpTransport("127.0.0.1:${server.localPort}"),"a".repeat(64)).updateFirmware(info,"123456",{bytes.inputStream()}){progress+=it}
            assertTrue(response.getBoolean("reboot"));assertEquals(0f,progress.first());assertEquals(1f,progress.last())
            worker.join(10000);assertFalse(worker.isAlive);failure.get()?.let{throw AssertionError(it)}
        }
    }
    private class FakeConnection(url:URL,val body:String,val status:Int=200):HttpURLConnection(url){
        val sent=ByteArrayOutputStream();var disconnected=false;var requestedResponse=false
        override fun connect()=Unit
        override fun usingProxy()=false
        override fun disconnect(){disconnected=true}
        override fun getOutputStream()=sent
        override fun getInputStream():InputStream=body.byteInputStream()
        override fun getErrorStream():InputStream=body.byteInputStream()
        override fun getResponseCode():Int{requestedResponse=true;return status}
    }
    @Test fun redirectsAndUnconfirmedResponsesAreNotSuccess()=runBlocking{
        val bytes=bundle();val info=FirmwareBundle.inspect{bytes.inputStream()}
        for((status,body) in listOf(302 to "",200 to "{\"ok\":true}",409 to "{\"ok\":false,\"error\":\"Abre la ventana física\"}")){
            lateinit var connection:FakeConnection;val progress=mutableListOf<Float>()
            val client=DeviceClient(HttpTransport("127.0.0.1:8080"){FakeConnection(it,body,status).also{created->connection=created}},"a".repeat(64))
            try{client.updateFirmware(info,"123456",{bytes.inputStream()}){progress+=it};fail("Expected rejected/unconfirmed update")}
            catch(_:DeviceFailure){}
            assertFalse(connection.instanceFollowRedirects);assertTrue(connection.disconnected);assertTrue(progress.none{it==1f})
        }
    }
    @Test fun cancellationClosesTheConnectionAndDoesNotConfirm()=runBlocking{
        val bytes=bundle(131072);val info=FirmwareBundle.inspect{bytes.inputStream()}
        lateinit var connection:FakeConnection;lateinit var transfer:Job
        val client=DeviceClient(HttpTransport("127.0.0.1:8080"){FakeConnection(it,"{\"ok\":true,\"reboot\":true}").also{created->connection=created}},"a".repeat(64))
        transfer=launch(start=CoroutineStart.LAZY){client.updateFirmware(info,"123456",{bytes.inputStream()}){if(it>0f)transfer.cancel()}}
        transfer.start();transfer.join()
        assertTrue(transfer.isCancelled);assertTrue(connection.disconnected);assertFalse(connection.requestedResponse)
        assertTrue(connection.sent.size()<bytes.size)
    }
}
