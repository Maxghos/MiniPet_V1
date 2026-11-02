package cl.maxghos.minipet

import android.content.Intent
import android.net.Uri
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import org.json.JSONArray
import org.json.JSONObject

@Composable private fun DateInput(value: String,enabled: Boolean,onChange:(String)->Unit){
    OutlinedTextField(value=value,onValueChange={onChange(it.filter(Char::isDigit).take(8))},label={Text("Fecha AAAAMMDD · vacío = hoy")},
        singleLine=true,enabled=enabled,keyboardOptions=KeyboardOptions(keyboardType=KeyboardType.Number),modifier=Modifier.fillMaxWidth())
}
@Composable fun DayPanel(model: MiniPetModel){
    val s=model.state
    LaunchedEffect(s.connected,s.additionalSupported,s.agenda=="{}"){if(s.connected&&s.additionalSupported&&s.agenda=="{}")model.loadDay()}
    if(!s.connected){Panel("Tu día, con MiniPet"){
        Text("Conecta tu MiniPet para organizar actividades, recordatorios, resumen diario y Telegram.")
        Button(onClick={model.edit{it.copy(tab=0)}}){Text("Ir a conectar")}
    };return}
    if(!s.additionalSupported){Panel("Actualiza a MiniPet 3.2"){
        Text("Instala el nuevo firmware para habilitar agenda, Telegram y pequeñas actividades espontáneas.")
    };return}
    val agenda=remember(s.agenda){JSONObject(s.agenda)};val today=agenda.optInt("day")
    val enabled=!s.busy;val ready=agenda.optBoolean("clockReady")
    var deletion by remember{mutableStateOf<Pair<String,Int>?>(null)}
    if(deletion!=null)AlertDialog(onDismissRequest={deletion=null},title={Text("¿Eliminar este elemento?")},
        text={Text("Se elimina de MiniPet y de la lista compartida con Telegram.")},
        confirmButton={TextButton(onClick={deletion?.let{model.dayCommand(it.first,JSONObject().put("id",it.second))};deletion=null},enabled=enabled){Text("Eliminar")}},
        dismissButton={TextButton(onClick={deletion=null}){Text("Cancelar")}})
    Row(Modifier.horizontalScroll(rememberScrollState()),horizontalArrangement=Arrangement.spacedBy(4.dp)){
        listOf("Actividades","Avisos","Resumen","Telegram").forEachIndexed{i,title->
            FilterChip(selected=s.dayTab==i,onClick={model.edit{it.copy(dayTab=i)}},label={Text(title)})
        }
    }
    Text(if(ready)"Hora de Santiago · ${dayLabel(today)}" else "Esperando hora de Santiago. Conecta MiniPet a internet para sincronizar.",style=MaterialTheme.typography.labelMedium)
    if(agenda.has("persisted")&&!agenda.optBoolean("persisted"))Text("La agenda está en memoria, pero no se pudo guardar para un reinicio.",color=MaterialTheme.colorScheme.error)
    OutlinedButton(onClick={model.loadDay()},enabled=enabled){Text("Sincronizar día y Telegram")}
    when(s.dayTab){
    0->{
        Panel("Actividades del día"){
            Text("Una lista compartida entre MiniPet, la app y tu bot. Hasta 24 actividades guardadas.")
            OutlinedTextField(value=s.taskTitle,onValueChange={v->if(v.toByteArray().size<=96)model.edit{it.copy(taskTitle=v.replace('\n',' '))}},label={Text("¿Qué quieres hacer?")},singleLine=true,enabled=enabled,modifier=Modifier.fillMaxWidth())
            DateInput(s.taskDay,enabled){v->model.edit{it.copy(taskDay=v)}}
            Button(onClick={model.addTask()},enabled=enabled&&ready&&s.taskTitle.isNotBlank(),modifier=Modifier.fillMaxWidth()){Text("Añadir actividad")}
            val tasks=agenda.optJSONArray("tasks")?:JSONArray()
            var all by remember{mutableStateOf(false)}
            Toggle("Ver también otras fechas",all){all=it}
            var shown=0
            for(i in 0 until tasks.length()){
                val t=tasks.getJSONObject(i);val day=t.optInt("day");val done=t.optBoolean("done");val id=t.getInt("id")
                if(!all&&day!=today&&(done||day>today))continue
                shown++
                Row(verticalAlignment=Alignment.CenterVertically){
                    Checkbox(checked=done,onCheckedChange={model.dayCommand("task.done",JSONObject().put("id",id).put("done",it))},enabled=enabled&&ready)
                    Column(Modifier.weight(1f)){
                        Text(t.getString("title"),fontWeight=FontWeight.SemiBold)
                        Text("ID $id · ${dayLabel(day)}${if(day<today&&!done)" · pendiente anterior" else ""}",style=MaterialTheme.typography.labelSmall)
                    }
                }
                Row{
                    if(!done&&day!=today)TextButton(onClick={model.dayCommand("task.today",JSONObject().put("id",id))},enabled=enabled&&ready){Text("Pasar a hoy")}
                    TextButton(onClick={deletion="task.delete" to id},enabled=enabled){Text("Eliminar")}
                }
                HorizontalDivider()
            }
            if(shown==0)Text("No hay actividades para mostrar. Puedes añadir la primera arriba.")
        }
    }
    1->{
        Panel("Recordatorios"){
            Text("Hasta 12 avisos puntuales o diarios. Se muestran cuando MiniPet está libre y pueden llegar también por Telegram.")
            OutlinedTextField(value=s.reminderTitle,onValueChange={v->if(v.toByteArray().size<=96)model.edit{it.copy(reminderTitle=v.replace('\n',' '))}},label={Text("Qué recordar")},singleLine=true,enabled=enabled,modifier=Modifier.fillMaxWidth())
            OutlinedTextField(value=s.reminderTime,onValueChange={v->model.edit{it.copy(reminderTime=v.take(5))}},label={Text("Hora HH:MM · Santiago")},singleLine=true,enabled=enabled,isError=parseClockMinute(s.reminderTime)==null,modifier=Modifier.fillMaxWidth())
            DateInput(s.reminderDay,enabled){v->model.edit{it.copy(reminderDay=v)}}
            Text("Sin fecha, se programa para la próxima vez que llegue esa hora: hoy o mañana.",style=MaterialTheme.typography.bodySmall)
            Toggle("Repetir cada día",s.reminderDaily,enabled){v->model.edit{it.copy(reminderDaily=v)}}
            Button(onClick={model.addReminder()},enabled=enabled&&ready&&s.reminderTitle.isNotBlank()&&parseClockMinute(s.reminderTime)!=null,modifier=Modifier.fillMaxWidth()){Text("Crear recordatorio")}
            val reminders=agenda.optJSONArray("reminders")?:JSONArray()
            if(reminders.length()==0)Text("Todavía no hay recordatorios.")
            for(i in 0 until reminders.length()){
                val r=reminders.getJSONObject(i);val id=r.getInt("id")
                Text(r.getString("title"),fontWeight=FontWeight.SemiBold)
                Text("${clockMinute(r.getInt("minute"))} · ${if(r.optBoolean("daily"))"diario desde" else "puntual"} ${dayLabel(r.getInt("day"))}${if(!r.optBoolean("enabled"))" · finalizado" else ""}")
                TextButton(onClick={deletion="reminder.delete" to id},enabled=enabled){Text("Eliminar aviso $id")};HorizontalDivider()
            }
        }
    }
    2->{
        Panel("Resumen diario"){
            Text(agenda.optString("summary","Sincroniza para ver tu resumen."))
            Text("Solo cuenta el tiempo de Pomodoro en marcha; las pausas y las esperas quedan fuera. El resumen reúne las sesiones del día.",style=MaterialTheme.typography.bodySmall)
            val days=agenda.optJSONArray("days")?:JSONArray()
            for(i in days.length()-1 downTo 0){val d=days.getJSONObject(i)
                HorizontalDivider();Text(dayLabel(d.getInt("day")),fontWeight=FontWeight.SemiBold)
                Text("${d.getLong("focusMs")/60000} min de estudio · ${d.getLong("breakMs")/60000} min de descanso\n${d.getInt("blocks")} bloques · ${d.getInt("tasks")} actividades completadas")
            }
            Text("Se conservan hasta 14 días. Sin hora sincronizada, los intervalos no se asignan a un día.",style=MaterialTheme.typography.bodySmall)
        }
        Panel("Avisos y compañía"){
            Toggle("Preparar resumen automático diario",s.dailySummary,enabled){v->model.editDayOptions{it.copy(dailySummary=v)}}
            OutlinedTextField(value=s.summaryTime,onValueChange={v->model.editDayOptions{it.copy(summaryTime=v.take(5))}},label={Text("Resumen a las HH:MM")},singleLine=true,isError=parseClockMinute(s.summaryTime)==null,enabled=enabled,modifier=Modifier.fillMaxWidth())
            Toggle("Pequeñas actividades espontáneas",s.scenes,enabled){v->model.editDayOptions{it.copy(scenes=v)}}
            Text("Leer un librito, respirar, estirarse o mirar una estrella. Son breves, dependen del humor y respetan No molestar y el modo estudio.")
            Button(onClick={model.saveDayOptions()},enabled=enabled&&parseClockMinute(s.summaryTime)!=null,modifier=Modifier.fillMaxWidth()){Text("Guardar opciones del día")}
        }
    }
    else->TelegramPanel(model)
    }
}
@Composable private fun TelegramPanel(model: MiniPetModel){
    val s=model.state;val t=remember(s.telegram){JSONObject(s.telegram)};val context=LocalContext.current
    var clear by remember{mutableStateOf(false)}
    if(clear)AlertDialog(onDismissRequest={clear=false},title={Text("¿Borrar la vinculación de Telegram?")},text={Text("Se eliminan el token y la cuenta autorizada del ESP32. Tu bot seguirá existiendo en Telegram.")},
        confirmButton={TextButton(onClick={model.clearTelegram();clear=false},enabled=!s.busy){Text("Borrar de MiniPet")}},dismissButton={TextButton(onClick={clear=false}){Text("Cancelar")}})
    Panel("Telegram personal"){
        val status=when{!t.optBoolean("configured")->"Sin configurar";!t.optBoolean("enabled",true)->"Desactivado";t.optInt("code")==401->"Token no válido: revisa BotFather";t.optInt("code")==409->"Otro cliente o webhook está usando este bot";!t.optBoolean("ready")->"Conectando: revisa Wi-Fi, hora y acceso a Telegram";!t.optBoolean("linked")->"Listo para vincular desde la OLED";else->"Vinculado a tu cuenta"}
        Text(status,fontWeight=FontWeight.Bold)
        Text("Crea un bot propio con @BotFather y copia su token. Lo introduces aquí una vez; MiniPet no lo devuelve al consultar el estado.")
        if(!s.bluetooth)Text("Para guardar el token, conecta la app por BLE cifrado. Después puedes usar Wi-Fi para la agenda y los ajustes.",color=MaterialTheme.colorScheme.secondary)
        OutlinedTextField(value=s.telegramToken,onValueChange={v->model.edit{it.copy(telegramToken=v.take(100))}},label={Text("Token de BotFather")},singleLine=true,
            enabled=s.bluetooth&&!s.busy,visualTransformation=PasswordVisualTransformation(),keyboardOptions=KeyboardOptions(keyboardType=KeyboardType.Password,autoCorrectEnabled=false),modifier=Modifier.fillMaxWidth())
        Button(onClick={model.configureTelegram()},enabled=s.bluetooth&&!s.busy&&s.telegramToken.isNotBlank(),modifier=Modifier.fillMaxWidth()){Text("Guardar token por BLE")}
        Text("Con MiniPet en internet, espera a que esté listo y abre Ajustes → Vincular Telegram en la OLED. Envía /vincular seguido de ese PIN al bot. Solo la cuenta vinculada podrá controlarlo.")
        val name=t.optString("username")
        if(name.matches(Regex("[A-Za-z0-9_]{5,64}")))OutlinedButton(onClick={context.startActivity(Intent(Intent.ACTION_VIEW,Uri.parse("https://t.me/$name")))},enabled=!s.busy){Text("Abrir @$name")}
        Text("Usa /ayuda para ver comandos y /estudio para los botones de Pomodoro. El ESP32 necesita Wi-Fi e internet; la app puede estar cerrada.")
        if(t.optBoolean("configured")){
            listOf("Activar Telegram" to "enabled","Avisos de Pomodoro" to "studyAlerts","Recordatorios" to "reminderAlerts","Resumen diario" to "summaryAlerts").forEach{(label,key)->
                Toggle(label,t.optBoolean(key,true),!s.busy){model.telegramFlag(key,it)}
            }
            Text("Los avisos automáticos respetan No molestar. Los mensajes antiguos se descartan al reconectar. El enlace del bot es público; su token es privado.",style=MaterialTheme.typography.bodySmall)
            TextButton(onClick={clear=true},enabled=!s.busy){Text("Borrar token y vinculación")}
        }
    }
}
