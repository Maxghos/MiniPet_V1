package cl.maxghos.minipet

import kotlinx.coroutines.runBlocking
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[28],manifest=Config.NONE)
class StudyTest {
    private fun stages()=JSONArray(javaClass.getResourceAsStream("/study_cycle.json")!!.bufferedReader().readText())
    @Test fun readsTheActualCppStudyCycleAndRecovery(){
        val values=stages();assertEquals(9,values.length())
        val states=List(values.length()){i->StudyStatus.parse(values.getJSONObject(i).getJSONObject("study"),1000)}
        assertEquals("idle",states[0].phase);assertEquals("focus",states[1].phase)
        assertEquals(1,states[2].remainingMs);assertEquals("shortBreak",states[3].phase)
        assertEquals("waiting",states[4].state);assertEquals("paused",states[5].state)
        assertEquals("recovered",states[6].event);assertEquals("paused",states[6].state)
        assertEquals("longBreak",states[7].phase);assertEquals(2,states[7].completedBlocks)
        assertFalse(states[8].active);assertEquals(120000,states[8].focusMs)
    }
    @Test fun countdownUsesElapsedTimeAndNeverInventsTheNextStage(){
        val s=StudyStatus.parse(stages().getJSONObject(1).getJSONObject("study"),1000)
        assertEquals(59000,s.remaining(2000));assertEquals(1000,s.focusTime(2000))
        assertEquals(0,s.remaining(1000000));assertEquals(60000,s.focusTime(1000000))
        assertEquals("focus",s.phase);assertEquals(0,s.completedBlocks);assertFalse(s.fresh(11000))
        assertEquals(60000,s.remaining(999));assertTrue(s.fresh(1000))
        val paused=StudyStatus.parse(stages().getJSONObject(5).getJSONObject("study"),1000)
        assertEquals(paused.remainingMs,paused.remaining(1000000));assertEquals(paused.focusMs,paused.focusTime(1000000))
        val waiting=StudyStatus.parse(stages().getJSONObject(4).getJSONObject("study"),1000)
        assertEquals(waiting.remainingMs,waiting.remaining(1000000))
    }
    @Test fun customProgramsHandleEmptyFieldsAndRespectDeviceLimits(){
        assertNull(StudyDraft(focus="").config());assertNull(StudyDraft(focus="0").config())
        assertNull(StudyDraft(focus="181").config());assertNull(StudyDraft(blocks="1").config())
        assertNull(StudyDraft(short="61").config());assertNull(StudyDraft(long="121").config())
        val config=StudyConfig(180,60,120,12,false,true)
        assertEquals(config,StudyDraft.from(config).config());assertEquals(config,StudyConfig.parse(config.json()))
        assertEquals("25:00",studyClock(1500000));assertEquals("00:01",studyClock(1));assertEquals("00:00",studyClock(-1))
        assertEquals("180:00",studyClock(10800000))
    }
    @Test fun commandsCarryTheRevisionAndStartProgramWithAuthentication()=runBlocking{
        val sent=mutableListOf<JSONObject>();val values=stages()
        val transport=object:CommandTransport{
            override val chunkSize=120
            override suspend fun exchange(command: JSONObject): JSONObject {
                sent+=command
                assertEquals("c".repeat(64),command.getString("token"))
                return JSONObject().put("ok",true).put("req",command.getInt("req"))
                    .put("study",values.getJSONObject(if(command.getString("op")=="study.get")0 else 1).getJSONObject("study"))
            }
            override fun close()=Unit
        }
        val client=StudyClient(DeviceClient(transport,"c".repeat(64)))
        val idle=client.get{1000};assertFalse(idle.active)
        val started=client.action("start",idle.revision,StudyConfig(1,1,2,2),{2000});assertTrue(started.active)
        assertEquals("study.start",sent.last().getString("op"));assertEquals(0,sent.last().getLong("expectedRevision"))
        assertEquals(1,sent.last().getJSONObject("config").getInt("focusMinutes"))
        client.action("pause",started.revision,null,{3000})
        assertEquals("study.pause",sent.last().getString("op"));assertFalse(sent.last().has("config"))
    }
    @Test fun staleStateRejectionRemainsAnErrorAndDoesNotRetryAnAction()=runBlocking{
        var calls=0
        val transport=object:CommandTransport{
            override val chunkSize=120
            override suspend fun exchange(command: JSONObject): JSONObject {calls++;return JSONObject().put("req",command.getInt("req")).put("ok",false).put("error","La etapa cambio")}
            override fun close()=Unit
        }
        try{StudyClient(DeviceClient(transport,"c".repeat(64))).action("skip",8,null,{0});fail("Expected a revision error")}
        catch(e:DeviceFailure){assertEquals("La etapa cambio",e.message)}
        assertEquals(1,calls)
    }
}
