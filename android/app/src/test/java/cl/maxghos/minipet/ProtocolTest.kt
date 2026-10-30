package cl.maxghos.minipet

import kotlinx.coroutines.runBlocking
import org.json.JSONObject
import org.json.JSONArray
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.net.ServerSocket
import java.util.Base64
import java.util.concurrent.atomic.AtomicReference
import kotlin.concurrent.thread

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[28],manifest=Config.NONE)
class ProtocolTest {
    @Test fun cppTextPagesMatchPhonePreview(){
        val cases=JSONArray(javaClass.getResourceAsStream("/text_layout.json")!!.bufferedReader().readText())
        repeat(cases.length()){index->
            val fixture=cases.getJSONObject(index);val pages=OledText.pages(fixture.getString("text"))
            assertEquals(fixture.getLong("duration"),pages.sumOf{it.duration})
            val expected=fixture.getJSONArray("pages");assertEquals(expected.length(),pages.size)
            pages.forEachIndexed{p,page->
                val exp=expected.getJSONObject(p);assertEquals(exp.getInt("top"),page.top);assertEquals(exp.getLong("duration"),page.duration)
                val lines=exp.getJSONArray("lines");assertEquals(lines.length(),page.lines.size)
                page.lines.forEachIndexed{l,line->val bytes=lines.getJSONArray(l);assertArrayEquals(ByteArray(bytes.length()){bytes.getInt(it).toByte()},line)}
            }
        }
    }
    @Test fun uploadUsesOffsetsAndCommitsOnlyAfterTheWholeAsset()=runBlocking{
        val sent=mutableListOf<String>();val received=java.io.ByteArrayOutputStream()
        val transport=object:CommandTransport{
            override val chunkSize=120
            override suspend fun exchange(command:JSONObject):JSONObject{
                val op=command.getString("op");sent+=op
                assertEquals("a".repeat(64),command.getString("token"))
                val reply=JSONObject().put("ok",true).put("req",command.getInt("req"))
                if(op=="upload.chunk"){
                    assertEquals(received.size(),command.getInt("offset"));received.write(Base64.getDecoder().decode(command.getString("data")))
                    reply.put("offset",received.size())
                }
                if(op=="upload.commit")MediaFormat.parse(received.toByteArray())
                return reply
            }
            override fun close()=Unit
        }
        val bytes=MediaFormat.pack(listOf(ByteArray(512){it.toByte()}),1)
        DeviceClient(transport,"a".repeat(64)).upload("sample","Prueba",bytes){}
        assertEquals("upload.begin",sent.first());assertEquals("upload.commit",sent.last());assertEquals(5,sent.count{it=="upload.chunk"})
        assertArrayEquals(bytes,received.toByteArray())
    }
    @Test fun failedUploadCancelsAndDoesNotCommit()=runBlocking{
        val sent=mutableListOf<String>()
        val transport=object:CommandTransport{
            override val chunkSize=120
            override suspend fun exchange(command:JSONObject):JSONObject{
                val op=command.getString("op");sent+=op
                if(op=="upload.chunk")throw DeviceFailure("Interrumpida")
                return JSONObject().put("ok",true).put("req",command.getInt("req"))
            }
            override fun close()=Unit
        }
        try{DeviceClient(transport,"a".repeat(64)).upload("sample","Prueba",MediaFormat.pack(listOf(ByteArray(512)),1)){};fail("Expected failure")}
        catch(_:DeviceFailure){}
        assertEquals(listOf("upload.begin","upload.chunk","upload.cancel"),sent)
    }
    @Test fun actualHttpRequestsCarryPairingAndAuthentication(): Unit=runBlocking{
        val failure=AtomicReference<Throwable?>()
        ServerSocket(0).use{server->
            server.soTimeout=10000
            val worker=thread{
                try{repeat(2){n->server.accept().use{socket->
                    socket.soTimeout=10000
                    val input=socket.getInputStream();val headers=StringBuilder()
                    while(!headers.endsWith("\r\n\r\n")){val c=input.read();check(c>=0);headers.append(c.toChar())}
                    assertTrue(headers.startsWith("POST /api/v1/command HTTP/1.1"))
                    val size=Regex("Content-Length: (\\d+)",RegexOption.IGNORE_CASE).find(headers)!!.groupValues[1].toInt()
                    val bytes=ByteArray(size);var pos=0;while(pos<size){val count=input.read(bytes,pos,size-pos);check(count>0);pos+=count}
                    val req=JSONObject(String(bytes,Charsets.UTF_8))
                    val reply=JSONObject().put("ok",true).put("req",req.getInt("req"))
                    if(n==0){assertEquals("pair",req.getString("op"));assertEquals("123456",req.getString("code"));reply.put("token","b".repeat(64))}
                    else{assertEquals("status",req.getString("op"));assertTrue(headers.contains("Authorization: Bearer ${"b".repeat(64)}"));reply.put("firmware","3.0.0")}
                    val body=reply.toString().toByteArray();val head="HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ${body.size}\r\nConnection: close\r\n\r\n"
                    socket.getOutputStream().write(head.toByteArray()+body)
                }}}
                catch(error:Throwable){failure.set(error)}
            }
            val client=DeviceClient(HttpTransport("127.0.0.1:${server.localPort}"))
            assertEquals("b".repeat(64),client.pair("123456"));assertEquals("3.0.0",client.call("status").getString("firmware"))
            worker.join(10000);assertFalse(worker.isAlive);failure.get()?.let{throw AssertionError(it)}
        }
    }
}
