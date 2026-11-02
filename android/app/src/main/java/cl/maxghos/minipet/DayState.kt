package cl.maxghos.minipet

import org.json.JSONObject

fun parseClockMinute(text: String): Int? {
    if(!text.matches(Regex("[0-9]{2}:[0-9]{2}")))return null
    val hour=text.substring(0,2).toInt();val minute=text.substring(3).toInt()
    return if(hour in 0..23&&minute in 0..59)hour*60+minute else null
}
fun clockMinute(value: Int)="%02d:%02d".format(java.util.Locale.ROOT,value.coerceIn(0,1439)/60,value.coerceIn(0,1439)%60)
fun dayLabel(day: Int): String=if(day<20240101)"Sin fecha" else "%02d/%02d/%04d".format(java.util.Locale.ROOT,day%100,day/100%100,day/10000)

class AdditionalClient(private val client: DeviceClient){
    suspend fun configureTelegram(token: String): JSONObject {
        require(client.transport.secureConfiguration){"El token solo se configura por BLE cifrado"}
        require(token.length<=100&&token.matches(Regex("[0-9]{5,16}:[A-Za-z0-9_-]{25,94}"))){"Revisa el token de BotFather"}
        return client.call("telegram.configure",JSONObject().put("tokenValue",token))
    }
}
