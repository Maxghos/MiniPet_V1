package cl.maxghos.minipet

import android.os.SystemClock
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay

private data class StudyConfirmation(val action: String,val revision: Long,val stage: String)
@Composable private fun studyNow(): Long {
    var now by remember{mutableLongStateOf(SystemClock.elapsedRealtime())}
    LaunchedEffect(Unit){while(true){delay(250);now=SystemClock.elapsedRealtime()}}
    return now
}
@Composable fun StudyShortcut(model: MiniPetModel){
    val s=model.state;val study=s.study?:return;val now=studyNow()
    OutlinedCard(Modifier.fillMaxWidth()){
        Row(Modifier.padding(12.dp),verticalAlignment=Alignment.CenterVertically){
            Column(Modifier.weight(1f)){
                Text("${study.label()} · ${studyClock(study.remaining(now))}",fontWeight=FontWeight.SemiBold)
                Text(if(!s.connected||!study.fresh(now))"Reconecta para confirmar la etapa" else when(study.state){"paused"->"En pausa";"waiting"->"Esperando tu confirmación";else->"MiniPet cuida el tiempo"},style=MaterialTheme.typography.labelSmall)
            }
            TextButton(onClick={model.edit{it.copy(tab=1)}}){Text("Ver")}
        }
    }
}
@Composable private fun StudyNumber(label: String,value: String,range: IntRange,enabled: Boolean,onChange: (String)->Unit){
    val valid=value.toIntOrNull() in range
    OutlinedTextField(value=value,onValueChange={onChange(it.filter(Char::isDigit).take(3))},label={Text(label)},
        supportingText={Text("${range.first}–${range.last}")},singleLine=true,isError=!valid,enabled=enabled,
        keyboardOptions=KeyboardOptions(keyboardType=KeyboardType.Number),modifier=Modifier.fillMaxWidth())
}
@Composable fun StudyPanel(model: MiniPetModel){
    val app=model.state;val s=app.study;val draft=app.studyDraft;val now=studyNow()
    val active=s?.active==true
    val enabled=app.connected&&app.studySupported&&!app.busy&&s?.fresh(now)==true
    var confirm by remember{mutableStateOf<StudyConfirmation?>(null)}
    if(confirm!=null){
        val pending=confirm!!;val skip=pending.action=="skip";val sameStage=s?.revision==pending.revision
        AlertDialog(onDismissRequest={confirm=null},title={Text(if(skip)"¿Saltar esta etapa?" else "¿Terminar la sesión?")},
            text={Text(if(!sameStage)"La etapa cambió mientras decidías. Cierra este aviso y vuelve a elegir la acción sobre el estado actualizado."
                else if(skip)"Se saltará: ${pending.stage}. El bloque de estudio que saltes no cuenta como completado. El tiempo que ya trabajaste se conserva en el resumen."
                else "MiniPet volverá a su comportamiento habitual. Puedes revisar tu resumen antes de empezar una sesión nueva.")},
            confirmButton={TextButton(onClick={model.studyAction(pending.action,pending.revision);confirm=null},enabled=enabled&&sameStage){Text(if(skip)"Confirmar salto" else "Confirmar fin")}},
            dismissButton={TextButton(onClick={confirm=null}){Text("Continuar aquí")}})
    }
    if(!app.connected)Panel("Conecta tu compañero"){
        Text("Conecta MiniPet para empezar o controlar una sesión. Si ya dejaste una en marcha, continúa en el ESP32 aunque cierres la app.")
        Button(onClick={model.edit{it.copy(tab=0)}}){Text("Ir a conectar")}
    }else if(!app.studySupported)Panel("Actualiza MiniPet"){
        Text("Este modo requiere el firmware MiniPet 3.1 o posterior. Actualiza el ESP32 con PlatformIO para habilitar estudio y descansos.")
    }
    Panel(if(active)s!!.label() else "Estudiemos por bloques"){
        Text(if(active)when(s!!.state){"paused"->"En pausa. Este tiempo no suma al estudio ni al descanso.";"waiting"->if(s.phase=="focus")"Todo listo. Empieza cuando quieras volver al estudio." else "Tu descanso está listo. Empieza cuando quieras.";else->if(s.phase=="focus")"Una cosa a la vez. Del tiempo me encargo yo." else "Levántate un momento, bebe agua o descansa la vista."}
            else "Concentración, una pausa de verdad y vuelta al estudio. El programa se ejecuta en MiniPet y funciona sin conexión.")
        if(active){
            val value=s!!
            val remaining=value.remaining(now)
            val progress=if(value.durationMs>0)(1f-remaining.toFloat()/value.durationMs).coerceIn(0f,1f) else 0f
            val ring=if(value.phase=="focus")MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.secondary
            val track=MaterialTheme.colorScheme.surfaceVariant
            Box(Modifier.fillMaxWidth().height(252.dp),contentAlignment=Alignment.Center){
                Canvas(Modifier.size(234.dp)){
                    val stroke=Stroke(8.dp.toPx(),cap=StrokeCap.Round);val inset=8.dp.toPx()
                    val bounds=Size(size.width-inset*2,size.height-inset*2)
                    drawArc(track,-90f,360f,false,Offset(inset,inset),bounds,style=stroke)
                    drawArc(ring,-90f,360f*progress,false,Offset(inset,inset),bounds,style=stroke)
                }
                Column(horizontalAlignment=Alignment.CenterHorizontally){
                    Text(studyClock(remaining),style=MaterialTheme.typography.displayLarge,fontWeight=FontWeight.Bold)
                    Text(when(value.state){"paused"->"EN PAUSA";"waiting"->"CUANDO ESTÉS LISTO";else->"MINUTOS : SEGUNDOS"},style=MaterialTheme.typography.labelSmall,color=ring)
                }
            }
            Text("Bloque ${value.round} de ${value.config.blocksBeforeLong} · ${value.completedBlocks} ${if(value.completedBlocks==1)"completado" else "completados"}",style=MaterialTheme.typography.labelLarge)
            if(!app.connected||!value.fresh(now))Text("Último estado recibido. Reconecta o sincroniza para confirmar la etapa actual.",color=MaterialTheme.colorScheme.secondary)
            if(!value.persisted)Text("No se pudo guardar la sesión para recuperarla tras un reinicio.",color=MaterialTheme.colorScheme.error)
            Button(onClick={model.studyAction(if(value.running)"pause" else "resume",value.revision)},enabled=enabled,modifier=Modifier.fillMaxWidth()){
                Text(if(value.running)"Pausar" else if(value.state=="waiting")"Empezar ${if(value.phase=="focus")"estudio" else "descanso"}" else "Reanudar")
            }
            Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.spacedBy(8.dp)){
                OutlinedButton(onClick={confirm=StudyConfirmation("skip",value.revision,value.label())},enabled=enabled,modifier=Modifier.weight(1f)){Text("Saltar etapa")}
                TextButton(onClick={confirm=StudyConfirmation("stop",value.revision,value.label())},enabled=enabled,modifier=Modifier.weight(1f)){Text("Terminar sesión")}
            }
        }else{
            Text("${draft.focus} min de estudio · ${draft.short} min de pausa · ${draft.long} min de pausa larga",style=MaterialTheme.typography.titleMedium)
            Button(onClick={model.studyAction("start",s?.revision)},enabled=enabled&&draft.config()!=null,modifier=Modifier.fillMaxWidth()){Text("Guardar y empezar a estudiar")}
        }
        if(app.connected&&app.studySupported)TextButton(onClick={model.syncStudy()},enabled=!app.busy){Text("Sincronizar con MiniPet")}
    }
    if(s!=null&&(s.completedBlocks>0||s.focusMs>0||s.breakMs>0))Panel(if(active)"Tu avance en esta sesión" else "Resumen de la última sesión"){
        Text("${s.completedBlocks} ${if(s.completedBlocks==1)"bloque completado" else "bloques completados"}",style=MaterialTheme.typography.headlineSmall)
        Text("Tiempo de estudio: ${studyClock(s.focusTime(now))}\nTiempo de descanso: ${studyClock(s.breakTime(now))}")
        Text("Las pausas no suman tiempo. Un bloque saltado conserva sus minutos trabajados, pero no suma como completado.",style=MaterialTheme.typography.bodySmall)
    }
    Panel("Tu programa"){
        if(active)Text("El programa queda fijo durante esta sesión. Termínala para preparar otros tiempos.")
        else{
            Text("Elige una base y ajústala a tu ritmo. Los cambios se guardan al pulsar Guardar o Empezar.")
            Row(horizontalArrangement=Arrangement.spacedBy(4.dp)){
                listOf("Clásico" to StudyConfig(),"Suave" to StudyConfig(15,5,15),"Profundo" to StudyConfig(50,10,20)).forEach{(label,config)->
                    OutlinedButton(onClick={model.studyPreset(config)},enabled=!app.busy,contentPadding=PaddingValues(horizontal=10.dp)){Text(label)}
                }
            }
        }
        val edit=!active&&!app.busy
        StudyNumber("Estudio · minutos",draft.focus,1..180,edit){v->model.editStudy{it.copy(focus=v)}}
        StudyNumber("Descanso corto · minutos",draft.short,1..60,edit){v->model.editStudy{it.copy(short=v)}}
        StudyNumber("Descanso largo · minutos",draft.long,1..120,edit){v->model.editStudy{it.copy(long=v)}}
        StudyNumber("Bloques antes del descanso largo",draft.blocks,2..12,edit){v->model.editStudy{it.copy(blocks=v)}}
        Toggle("Empezar descansos automáticamente",draft.autoBreak,edit){v->model.editStudy{it.copy(autoBreak=v)}}
        Toggle("Volver a estudiar automáticamente",draft.autoFocus,edit){v->model.editStudy{it.copy(autoFocus=v)}}
        Text(if(draft.autoFocus)"Al terminar un descanso empezará el siguiente bloque de estudio." else "Al terminar un descanso, MiniPet te esperará hasta que pulses Empezar estudio.",style=MaterialTheme.typography.bodySmall)
        if(!active)OutlinedButton(onClick={model.studyAction("configure",s?.revision)},enabled=enabled&&draft.config()!=null,modifier=Modifier.fillMaxWidth()){Text("Guardar programa sin empezar")}
    }
    Panel("MiniPet te acompaña"){
        Text("Durante la sesión no hay bromas aleatorias, solicitudes de atención ni consultas por movimiento que tapen el tiempo. Los ojos y los avisos cambian entre concentración y descanso.")
        Text("Puedes cerrar la app: el temporizador y los avisos visuales siguen en la OLED. Tras un reinicio se recupera pausado, con hasta un minuto de diferencia desde el último guardado.")
        Text("Desde el botón: Ajustes → Modo estudio. Puedes empezar, pausar, continuar, saltar, terminar o ajustar el programa sin teléfono. La app muestra cambios de etapa mientras está abierta y conectada.",style=MaterialTheme.typography.bodySmall)
    }
}
