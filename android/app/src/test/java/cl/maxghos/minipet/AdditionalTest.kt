package cl.maxghos.minipet

import kotlinx.coroutines.runBlocking
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[28],manifest=Config.NONE)
class AdditionalTest {
    @Test fun cppAgendaHasActivitiesRemindersAndDailyTotals(){
        val data=JSONObject(javaClass.getResourceAsStream("/additional.json")!!.bufferedReader().readText())
        val agenda=data.getJSONObject("agenda")
        assertTrue(agenda.getBoolean("clockReady"));assertEquals(20261102,agenda.getInt("day"))
        assertEquals("Repasar algebra",agenda.getJSONArray("tasks").getJSONObject(0).getString("title"))
        assertEquals(620,agenda.getJSONArray("reminders").getJSONObject(0).getInt("minute"))
        assertTrue(agenda.getJSONArray("reminders").getJSONObject(0).getBoolean("daily"))
        assertEquals(1500000,agenda.getJSONArray("days").getJSONObject(0).getLong("focusMs"))
        assertTrue(agenda.getString("summary").contains("Estudio: 25 min"))
        val telegram=data.getJSONObject("telegram");assertTrue(telegram.getBoolean("configured"));assertTrue(telegram.getBoolean("linked"))
        for(secret in listOf("token","tokenValue","chat","user","pin"))assertFalse(telegram.has(secret))
    }
    @Test fun clockAndDatesUseExplicitSantiagoHours(){
        assertEquals(0,parseClockMinute("00:00"));assertEquals(1439,parseClockMinute("23:59"))
        for(bad in listOf("24:00","13:60","9:15","12:3a","","12:00 extra"))assertNull(parseClockMinute(bad))
        assertEquals("20:30",clockMinute(1230));assertEquals("02/11/2026",dayLabel(20261102))
    }
    @Test fun tokenIsNeverSentOverHttpEvenIfTheCallerTries()=runBlocking{
        var called=false
        val transport=object:CommandTransport {
            override val chunkSize=3072
            override suspend fun exchange(command: JSONObject): JSONObject {called=true;return JSONObject()}
            override fun close()=Unit
        }
        try{AdditionalClient(DeviceClient(transport)).configureTelegram("123456789"+":"+"A".repeat(35));fail("Expected secure transport requirement")}
        catch(_:IllegalArgumentException){}
        assertFalse(called)
    }
    @Test fun encryptedTransportCarriesOnlyAnExplicitConfigurationRequest()=runBlocking{
        val credential="a".repeat(64);val botToken="123456789"+":"+"A".repeat(35);var received: JSONObject?=null
        val transport=object:CommandTransport {
            override val chunkSize=120
            override val secureConfiguration=true
            override suspend fun exchange(command: JSONObject): JSONObject {
                received=command;return JSONObject().put("ok",true).put("req",command.getInt("req")).put("telegram",JSONObject().put("configured",true))
            }
            override fun close()=Unit
        }
        AdditionalClient(DeviceClient(transport,credential)).configureTelegram(botToken)
        assertEquals("telegram.configure",received!!.getString("op"));assertEquals(botToken,received!!.getString("tokenValue"));assertEquals(credential,received!!.getString("token"))
    }
}
