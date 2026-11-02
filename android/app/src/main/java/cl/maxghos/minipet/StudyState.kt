package cl.maxghos.minipet

import org.json.JSONObject

data class StudyConfig(
    val focusMinutes: Int=25,val shortBreakMinutes: Int=5,val longBreakMinutes: Int=15,
    val blocksBeforeLong: Int=4,val autoBreak: Boolean=true,val autoFocus: Boolean=false
) {
    fun valid()=focusMinutes in 1..180&&shortBreakMinutes in 1..60&&longBreakMinutes in 1..120&&blocksBeforeLong in 2..12
    fun json(): JSONObject {
        require(valid()){"Revisa los tiempos del programa"}
        return JSONObject().put("focusMinutes",focusMinutes).put("shortBreakMinutes",shortBreakMinutes)
            .put("longBreakMinutes",longBreakMinutes).put("blocksBeforeLong",blocksBeforeLong)
            .put("autoBreak",autoBreak).put("autoFocus",autoFocus)
    }
    companion object {
        fun parse(value: JSONObject)=StudyConfig(value.getInt("focusMinutes"),value.getInt("shortBreakMinutes"),
            value.getInt("longBreakMinutes"),value.getInt("blocksBeforeLong"),value.getBoolean("autoBreak"),
            value.getBoolean("autoFocus")).also{require(it.valid()){"Programa de estudio inválido"}}
    }
}
data class StudyDraft(
    val focus: String="25",val short: String="5",val long: String="15",val blocks: String="4",
    val autoBreak: Boolean=true,val autoFocus: Boolean=false
) {
    fun config(): StudyConfig? {
        val result=StudyConfig(focus.toIntOrNull()?:return null,short.toIntOrNull()?:return null,
            long.toIntOrNull()?:return null,blocks.toIntOrNull()?:return null,autoBreak,autoFocus)
        return result.takeIf{it.valid()}
    }
    companion object {
        fun from(value: StudyConfig)=StudyDraft(value.focusMinutes.toString(),value.shortBreakMinutes.toString(),
            value.longBreakMinutes.toString(),value.blocksBeforeLong.toString(),value.autoBreak,value.autoFocus)
    }
}
data class StudyStatus(
    val config: StudyConfig,val phase: String,val state: String,val remainingMs: Long,val durationMs: Long,
    val completedBlocks: Int,val round: Int,val focusMs: Long,val breakMs: Long,val revision: Long,
    val eventId: Long,val event: String,val eventAgeMs: Long,val persisted: Boolean,val receivedAt: Long
) {
    val active get()=phase!="idle"
    val running get()=state=="running"
    fun elapsed(now: Long)=(now-receivedAt).coerceAtLeast(0)
    fun remaining(now: Long)=(remainingMs-if(running)elapsed(now) else 0).coerceAtLeast(0)
    fun focusTime(now: Long)=focusMs+if(running&&phase=="focus")elapsed(now).coerceAtMost(remainingMs) else 0
    fun breakTime(now: Long)=breakMs+if(running&&phase!="focus")elapsed(now).coerceAtMost(remainingMs) else 0
    fun fresh(now: Long)=elapsed(now)<10000
    fun label()=when(phase){"focus"->"Concentración";"shortBreak"->"Descanso corto";"longBreak"->"Descanso largo";else->"Tu próxima sesión"}
    companion object {
        fun parse(value: JSONObject,receivedAt: Long): StudyStatus {
            val result=StudyStatus(StudyConfig.parse(value.getJSONObject("config")),value.getString("phase"),
                value.getString("state"),value.getLong("remainingMs"),value.getLong("durationMs"),value.getInt("completedBlocks"),
                value.getInt("round"),value.getLong("focusMs"),value.getLong("breakMs"),value.getLong("revision"),
                value.getLong("eventId"),value.getString("event"),value.getLong("eventAgeMs"),value.optBoolean("persisted",true),receivedAt)
            require(result.phase in listOf("idle","focus","shortBreak","longBreak")&&result.state in listOf("idle","running","paused","waiting")&&
                result.remainingMs in 0..10800000&&result.durationMs in 0..10800000&&result.remainingMs<=result.durationMs&&
                result.completedBlocks>=0&&result.focusMs>=0&&result.breakMs>=0&&result.round in 1..result.config.blocksBeforeLong&&
                result.revision in 0..0xffffffffL&&result.eventId in 0..0xffffffffL){"Estado de estudio inválido"}
            return result
        }
    }
}
fun studyClock(milliseconds: Long): String {
    val seconds=(milliseconds.coerceAtLeast(0)+999)/1000
    return "%02d:%02d".format(java.util.Locale.ROOT,seconds/60,seconds%60)
}
class StudyClient(private val device: DeviceClient) {
    suspend fun get(receivedAt: ()->Long): StudyStatus=StudyStatus.parse(device.call("study.get").getJSONObject("study"),receivedAt())
    suspend fun action(action: String,revision: Long,config: StudyConfig?,receivedAt: ()->Long): StudyStatus {
        require(action in listOf("start","configure","pause","resume","skip","stop"))
        val fields=JSONObject().put("expectedRevision",revision)
        config?.let{fields.put("config",it.json())}
        return StudyStatus.parse(device.call("study.$action",fields).getJSONObject("study"),receivedAt())
    }
}
