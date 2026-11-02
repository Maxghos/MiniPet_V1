package cl.maxghos.minipet

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import org.json.JSONArray
import org.json.JSONObject

private fun String.fitsBytes(limit: Int)=toByteArray(Charsets.UTF_8).size<=limit
@Composable fun HabitsPanel(model: MiniPetModel){
    val s=model.state;val data=remember(s.habits){JSONArray(s.habits)}
    val ready=remember(s.agenda){JSONObject(s.agenda).optBoolean("clockReady")};val enabled=!s.busy
    var remove by remember{mutableStateOf<Int?>(null)}
    if(remove!=null)AlertDialog(onDismissRequest={remove=null},title={Text("¿Eliminar este hábito?")},
        text={Text("Se eliminará también su registro de días completados.")},
        confirmButton={TextButton(onClick={remove?.let{model.expansionCommand("habit.delete",JSONObject().put("id",it))};remove=null},enabled=enabled){Text("Eliminar hábito")}},
        dismissButton={TextButton(onClick={remove=null}){Text("Conservar")}})
    Panel("Hábitos y metas semanales"){
        Text("Completa cada hábito una vez al día. La meta cuenta días de lunes a domingo; la racha muestra días consecutivos. Hasta ocho hábitos.")
        OutlinedTextField(value=s.habitTitle,onValueChange={value->if(value.fitsBytes(96))model.edit{it.copy(habitTitle=value.replace('\n',' '))}},label={Text("Nuevo hábito")},singleLine=true,enabled=enabled,modifier=Modifier.fillMaxWidth())
        OutlinedTextField(value=s.habitTarget,onValueChange={value->model.edit{it.copy(habitTarget=value.filter(Char::isDigit).take(1))}},label={Text("Meta: días por semana (1–7)")},
            keyboardOptions=KeyboardOptions(keyboardType=KeyboardType.Number),singleLine=true,enabled=enabled,isError=s.habitTarget.toIntOrNull() !in 1..7,modifier=Modifier.fillMaxWidth())
        Button(onClick={model.addHabit()},enabled=enabled&&ready&&data.length()<8&&s.habitTitle.isNotBlank()&&s.habitTarget.toIntOrNull() in 1..7,modifier=Modifier.fillMaxWidth()){Text("Crear hábito")}
        if(data.length()==0)Text("Empieza con algo pequeño, como leer diez minutos.")
        for(i in 0 until data.length()){
            val h=data.getJSONObject(i);val id=h.getInt("id");val target=h.optInt("targetPerWeek",1).coerceIn(1,7);val done=h.optInt("weekDone")
            HorizontalDivider()
            Row(verticalAlignment=Alignment.CenterVertically){
                Checkbox(checked=h.optBoolean("doneToday"),onCheckedChange={model.expansionCommand("habit.done",JSONObject().put("id",id).put("done",it))},enabled=enabled&&ready)
                Column(Modifier.weight(1f)){
                    Text(h.getString("title"),fontWeight=FontWeight.SemiBold)
                    Text(if(h.optBoolean("doneToday"))"Hoy completado" else "Pendiente hoy",style=MaterialTheme.typography.labelMedium)
                }
            }
            LinearProgressIndicator(progress={(done.toFloat()/target).coerceIn(0f,1f)},modifier=Modifier.fillMaxWidth())
            Text("Semana: $done / $target días · Racha: ${h.optInt("streak")} días${if(h.optBoolean("goalReached"))" · ¡Meta conseguida!" else ""}")
            TextButton(onClick={remove=id},enabled=enabled){Text("Eliminar hábito ${h.getString("title")}")}
        }
        Text("Los cambios se comparten con Telegram. Sin fecha sincronizada no se asignan logros a un día.",style=MaterialTheme.typography.bodySmall)
    }
}
@Composable fun RoutinesPanel(model: MiniPetModel){
    val s=model.state;val data=remember(s.routines){JSONArray(s.routines)};val enabled=!s.busy
    val ready=remember(s.agenda){JSONObject(s.agenda).optBoolean("clockReady")}
    var remove by remember{mutableStateOf<Int?>(null)};var run by remember{mutableStateOf<JSONObject?>(null)}
    if(remove!=null)AlertDialog(onDismissRequest={remove=null},title={Text("¿Eliminar esta rutina?")},text={Text("Las actividades que ya creó para hoy se conservan.")},
        confirmButton={TextButton(onClick={remove?.let{model.expansionCommand("routine.delete",JSONObject().put("id",it))};remove=null},enabled=enabled){Text("Eliminar rutina")}},
        dismissButton={TextButton(onClick={remove=null}){Text("Conservar")}})
    if(run!=null)AlertDialog(onDismissRequest={run=null},title={Text("¿Iniciar ${run!!.getString("title")}?")},
        text={Text("Añade sus actividades para hoy${if(run!!.optBoolean("overview"))", consulta clima y hora" else ""}${if(run!!.optBoolean("focus"))" e inicia el programa de estudio guardado" else ""}. No repite actividades con el mismo título y fecha.")},
        confirmButton={TextButton(onClick={run?.let{model.expansionCommand("routine.run",JSONObject().put("id",it.getInt("id")))};run=null},enabled=enabled){Text("Iniciar rutina")}},
        dismissButton={TextButton(onClick={run=null}){Text("Ahora no")}})
    val tasks=s.routineTasks.lines().map(String::trim).filter(String::isNotEmpty)
    Panel(if(s.routineId==null)"Crear una rutina" else "Editar rutina"){
        Text("Prepara tu lista y elige una consulta de clima/hora o Pomodoro. Hasta seis rutinas y seis actividades en cada una.")
        OutlinedTextField(value=s.routineTitle,onValueChange={value->if(value.fitsBytes(96))model.edit{it.copy(routineTitle=value.replace('\n',' '))}},label={Text("Nombre de la rutina")},singleLine=true,enabled=enabled,modifier=Modifier.fillMaxWidth())
        OutlinedTextField(value=s.routineTasks,onValueChange={value->if(value.fitsBytes(640))model.edit{it.copy(routineTasks=value)}},label={Text("Actividades, una por línea")},minLines=3,maxLines=7,enabled=enabled,
            isError=tasks.size>6||tasks.any{!it.fitsBytes(96)},modifier=Modifier.fillMaxWidth())
        Toggle("Mostrar clima y hora",s.routineOverview,enabled){value->model.edit{it.copy(routineOverview=value,routineFocus=if(value)false else it.routineFocus)}}
        Toggle("Iniciar Pomodoro guardado",s.routineFocus,enabled){value->model.edit{it.copy(routineFocus=value,routineOverview=if(value)false else it.routineOverview)}}
        Text("Elige consulta o estudio para acompañar la lista; también puedes crear solo actividades.",style=MaterialTheme.typography.bodySmall)
        Button(onClick={model.saveRoutine()},enabled=enabled&&s.routineTitle.isNotBlank()&&tasks.size<=6&&tasks.all{it.fitsBytes(96)}&&(tasks.isNotEmpty()||s.routineFocus||s.routineOverview)&&(s.routineId!=null||data.length()<6),modifier=Modifier.fillMaxWidth()){Text(if(s.routineId==null)"Guardar nueva rutina" else "Guardar cambios de rutina")}
        if(s.routineId!=null)TextButton(onClick={model.clearRoutineDraft()},enabled=enabled){Text("Cancelar edición")}
    }
    Panel("Tus rutinas"){
        if(data.length()==0)Text("Guarda una rutina para empezar tu día con una sola acción.")
        for(i in 0 until data.length()){
            val r=data.getJSONObject(i);val list=r.optJSONArray("tasks")?:JSONArray()
            Text(r.getString("title"),fontWeight=FontWeight.Bold)
            Text("${list.length()} actividades${if(r.optBoolean("overview"))" · clima y hora" else ""}${if(r.optBoolean("focus"))" · estudio" else ""}")
            for(j in 0 until list.length())Text("• ${list.getString(j)}",style=MaterialTheme.typography.bodySmall)
            Button(onClick={run=r},enabled=enabled&&ready&&s.study?.active!=true){Text("Preparar ${r.getString("title")}")}
            Row{
                TextButton(onClick={model.editRoutine(r)},enabled=enabled){Text("Editar ${r.getString("title")}")}
                TextButton(onClick={remove=r.getInt("id")},enabled=enabled){Text("Eliminar ${r.getString("title")}")}
            }
            HorizontalDivider()
        }
    }
}
@Composable fun CompanionProfilePanel(model: MiniPetModel){
    val s=model.state;val c=remember(s.companion){JSONObject(s.companion)};val enabled=!s.busy
    Panel("Tu historia con MiniPet"){
        Text("MiniPet recuerda logros y pequeños relatos entre días. Sus intervenciones siguen el humor y respetan No molestar y el estudio.")
        OutlinedTextField(value=s.companionName,onValueChange={value->if(value.fitsBytes(24))model.editCompanion{it.copy(companionName=value.replace('\n',' '))}},label={Text("Nombre de la mascota")},singleLine=true,enabled=enabled,modifier=Modifier.fillMaxWidth())
        OutlinedTextField(value=s.companionNickname,onValueChange={value->if(value.fitsBytes(24))model.editCompanion{it.copy(companionNickname=value.replace('\n',' '))}},label={Text("Cómo quieres que te llame (opcional)")},singleLine=true,enabled=enabled,modifier=Modifier.fillMaxWidth())
        Toggle("Saludos según la hora",s.companionGreetings,enabled){value->model.editCompanion{it.copy(companionGreetings=value)}}
        Toggle("Celebrar mis logros",s.companionCelebrations,enabled){value->model.editCompanion{it.copy(companionCelebrations=value)}}
        Toggle("Pequeñas historias entre días",s.companionStories,enabled){value->model.editCompanion{it.copy(companionStories=value)}}
        Button(onClick={model.saveCompanion()},enabled=enabled&&s.companionName.isNotBlank(),modifier=Modifier.fillMaxWidth()){Text("Guardar compañía")}
        HorizontalDivider()
        Text("Días juntos: ${c.optInt("daysTogether")}\nBloques completados: ${c.optInt("totalBlocks")}\nActividades completadas: ${c.optInt("totalTasks")}")
        Text("Son logros registrados por MiniPet; no mide tu atención ni te penaliza por ausentarte.",style=MaterialTheme.typography.bodySmall)
    }
}
@Composable fun GamesPanel(model: MiniPetModel){
    val s=model.state;val g=remember(s.game){JSONObject(s.game)};val enabled=!s.busy&&s.study?.active!=true
    Panel("Un descanso con la mano"){
        Text("Dos juegos cortos en la OLED. La distancia de tu mano al HC-SR05 controla el movimiento; el botón sigue reservado para ajustes.")
        Text("Coloca la mano entre 15 y 80 cm, con la palma orientada al sensor. Cada partida comienza con una cuenta atrás.")
        Text("Puntería: mantén la mano en la distancia objetivo. Esquivar: acerca y aleja la mano para evitar obstáculos.")
        if(g.optBoolean("active")){
            Text("Partida: ${if(g.optString("kind")=="dodge")"Esquivar" else "Puntería"} · ${g.optInt("score")} puntos",fontWeight=FontWeight.Bold)
            Text("Sigue la OLED para el tiempo y la posición. Si se pierde el sensor, la partida deja de puntuar.")
            Button(onClick={model.expansionCommand("game.stop")},enabled=!s.busy){Text("Terminar juego")}
        }else{
            Button(onClick={model.expansionCommand("game.start",JSONObject().put("kind","target"))},enabled=enabled,modifier=Modifier.fillMaxWidth()){Text("Jugar Puntería")}
            OutlinedButton(onClick={model.expansionCommand("game.start",JSONObject().put("kind","dodge"))},enabled=enabled,modifier=Modifier.fillMaxWidth()){Text("Jugar Esquivar")}
            if(g.has("score"))Text("Último resultado: ${g.optInt("score")} puntos")
        }
        Text("Solo se inician cuando tú los eliges. Puedes abrir ajustes para salir. Termina Pomodoro antes de jugar.",style=MaterialTheme.typography.bodySmall)
    }
}
@Composable fun FirmwarePanel(model: MiniPetModel){
    val s=model.state;val ota=remember(s.ota){JSONObject(s.ota)}
    val picker=rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()){uri->uri?.let{model.chooseFirmware(it)}}
    var confirm by remember{mutableStateOf(false)}
    if(confirm)AlertDialog(onDismissRequest={confirm=false},title={Text("¿Instalar firmware ${s.firmwareInfo?.version}?")},
        text={Text("Mantén MiniPet encendido y el teléfono conectado a su Wi-Fi. Al validar el paquete, la mascota se reiniciará. Después vuelve a conectar para comprobar la versión.")},
        confirmButton={TextButton(onClick={confirm=false;model.updateFirmware()},enabled=!s.busy){Text("Enviar e instalar")}},
        dismissButton={TextButton(onClick={confirm=false}){Text("Cancelar")}})
    Panel("Actualizar por Wi-Fi"){
        Text("La primera instalación de MiniPet 3.3 se hace por USB. A partir de entonces puedes cargar paquetes .mpfw desde aquí, en la misma red local.")
        if(!s.connected)Text("Conecta MiniPet por Wi-Fi para consultar su versión y autorizar la actualización.")
        else{
            Text("Firmware actual: ${s.firmwareVersion.ifBlank{"No informado"}}")
            if(!s.expansionSupported)Text("Esta versión del firmware necesita la primera actualización por USB.")
            else if(!s.wifiUpdate)Text("La conexión activa es BLE. Reconecta por Wi-Fi para enviar el firmware.")
            OutlinedButton(onClick={model.refreshOta()},enabled=!s.busy&&s.expansionSupported){Text("Consultar ventana de actualización")}
            val phase=when(ota.optString("phase")){"idle"->"En espera";"ready"->"Listo";"receiving"->"Recibiendo";"error"->"Revisa el aviso";"rebooting"->"Reiniciando";else->"Sin consultar"}
            if(ota.has("phase"))Text("Estado: $phase · ${if(ota.optBoolean("window"))"ventana autorizada (${ota.optInt("seconds")} s al consultar)" else "ventana cerrada"}")
            if(ota.optString("error").isNotBlank())Text(ota.optString("error"),color=MaterialTheme.colorScheme.error)
        }
        Button(onClick={picker.launch(arrayOf("application/octet-stream","application/x-minipet-firmware","*/*"))},enabled=!s.busy,modifier=Modifier.fillMaxWidth()){Text("Elegir paquete .mpfw")}
        s.firmwareInfo?.let{info->
            Text("Versión ${info.version}\nVariante: ${info.target}\nImagen: ${(info.imageBytes+1023)/1024} KiB\nSHA-256 comprobado",style=MaterialTheme.typography.bodyMedium)
        }
        Text("En la OLED: Ajustes → Actualizar WiFi. Escribe aquí el código temporal de seis cifras. No es el PIN de vinculación de la app.")
        OutlinedTextField(value=s.firmwareCode,onValueChange={value->model.edit{it.copy(firmwareCode=value.filter(Char::isDigit).take(6))}},label={Text("Código de actualización de la OLED")},
            keyboardOptions=KeyboardOptions(keyboardType=KeyboardType.NumberPassword),visualTransformation=PasswordVisualTransformation(),singleLine=true,enabled=!s.busy,modifier=Modifier.fillMaxWidth())
        Button(onClick={confirm=true},enabled=s.connected&&s.expansionSupported&&s.wifiUpdate&&!s.busy&&s.study?.active!=true&&s.firmwareInfo!=null&&s.firmwareCode.length==6,modifier=Modifier.fillMaxWidth()){Text("Revisar e instalar firmware")}
        Text("El paquete conserva biblioteca y ajustes. Una interrupción antes de validar deja la versión activa; si la respuesta se pierde al terminar, reconecta para comprobar qué versión arrancó.",style=MaterialTheme.typography.bodySmall)
    }
}
