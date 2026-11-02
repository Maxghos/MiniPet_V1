package cl.maxghos.minipet

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay
import org.json.JSONObject

class MainActivity: ComponentActivity() {
    private val model: MiniPetModel by viewModels()
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState);enableEdgeToEdge()
        setContent { MiniPetApp(model) }
    }
    override fun onStart(){super.onStart();model.foreground(true)}
    override fun onStop(){model.foreground(false);super.onStop()}
}
private val Mint=Color(0xffa3f4cd)
private val Colors=darkColorScheme(primary=Mint,onPrimary=Color(0xff123727),secondary=Color(0xffffcca2),background=Color(0xff101a21),surface=Color(0xff182731),surfaceVariant=Color(0xff233640))
@OptIn(ExperimentalMaterial3Api::class)
@Composable fun MiniPetApp(model: MiniPetModel) {
    val state=model.state
    val scroll=rememberScrollState()
    LaunchedEffect(state.tab,state.sendTab,state.dayTab,state.routineId,state.study?.active){scroll.scrollTo(0)}
    MaterialTheme(colorScheme=Colors){
        Scaffold(topBar={TopAppBar(title={Column{Text("MiniPet",fontWeight=FontWeight.Bold);Text(if(state.connected)state.connectionName else "Tu compañero, a tu manera",style=MaterialTheme.typography.labelMedium)}})},
            bottomBar={NavigationBar{listOf("Conectar","Estudio","Día","Enviar","Ajustes").forEachIndexed{i,title->
                NavigationBarItem(selected=state.tab==i,onClick={model.edit{it.copy(tab=i)}},icon={Text(listOf("◎","◷","☑","Aa","⚙")[i])},label={Text(title)})
            }}}){padding->
            Column(Modifier.fillMaxSize().padding(padding).verticalScroll(scroll).padding(18.dp),verticalArrangement=Arrangement.spacedBy(16.dp)){
                if(state.notice.isNotBlank())Card(colors=CardDefaults.cardColors(containerColor=if(state.error)MaterialTheme.colorScheme.errorContainer else MaterialTheme.colorScheme.surfaceVariant)){
                    Text(state.notice,Modifier.padding(14.dp),style=MaterialTheme.typography.bodyMedium)
                }
                if(state.busy){
                    if(state.progress>0)LinearProgressIndicator(progress={state.progress},modifier=Modifier.fillMaxWidth()) else LinearProgressIndicator(Modifier.fillMaxWidth())
                    TextButton(onClick={model.cancel()}){Text("Cancelar operación")}
                }
                if(state.tab!=1&&state.study?.active==true)StudyShortcut(model)
                when(state.tab){0->ConnectionPanel(model);1->StudyPanel(model);2->DayPanel(model);3->SendPanel(model);4->SettingsPanel(model)}
                Spacer(Modifier.height(12.dp))
            }
        }
    }
}
@Composable fun Panel(title: String,body: @Composable ColumnScope.()->Unit){
    Card(shape=RoundedCornerShape(20.dp),modifier=Modifier.fillMaxWidth()){
        Column(Modifier.padding(18.dp),verticalArrangement=Arrangement.spacedBy(12.dp)){
            Text(title,style=MaterialTheme.typography.titleLarge,fontWeight=FontWeight.SemiBold);body()
        }
    }
}
@Composable fun Toggle(label: String,checked: Boolean,enabled: Boolean=true,onChange:(Boolean)->Unit){
    Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically){Text(label,Modifier.weight(1f));Switch(checked=checked,onCheckedChange=onChange,enabled=enabled)}
}
@Composable private fun Control(label: String,value: Float,range: ClosedFloatingPointRange<Float>,steps: Int=0,enabled: Boolean=true,onChange:(Float)->Unit){
    Column{Text(label,style=MaterialTheme.typography.bodyMedium);Slider(value=value.coerceIn(range.start,range.endInclusive),onValueChange=onChange,valueRange=range,steps=steps,enabled=enabled)}
}
@Composable private fun PixelPreview(key: Any?,frame:(Long)->ByteArray?){
    var age by remember{mutableLongStateOf(0)}
    LaunchedEffect(key){val start=android.os.SystemClock.elapsedRealtime();while(true){age=android.os.SystemClock.elapsedRealtime()-start;delay(66)}}
    val pixels=frame(age)
    Column(verticalArrangement=Arrangement.spacedBy(8.dp)){
        Canvas(Modifier.fillMaxWidth().aspectRatio(4f).background(Color.Black,RoundedCornerShape(8.dp))){
            val sx=size.width/128;val sy=size.height/32
            if(pixels!=null)for(y in 0..31)for(x in 0..127)if((pixels[x+(y/8)*128].toInt() and (1 shl (y%8)))!=0)
                drawRect(Mint,Offset(x*sx,y*sy),Size(sx,sy))
        }
        Text("Vista real · 128 × 32 · monocromo",style=MaterialTheme.typography.labelSmall,color=Mint)
    }
}
@Composable private fun ConnectionPanel(model: MiniPetModel){
    val s=model.state;val context=LocalContext.current
    val permissions=rememberLauncherForPermissions{model.discoverBle()}
    Panel(if(s.connected)"MiniPet conectado" else "Vincula tu compañero"){
        Text("Conectar la app no cambia lo que hace MiniPet. Solo mostrar contenido o aplicar ajustes cuenta como interacción.")
        if(!s.connected){
            Text("En la OLED: Ajustes → Vincular app. Introduce su PIN; dura dos minutos.",style=MaterialTheme.typography.bodyMedium)
            OutlinedTextField(value=s.pin,onValueChange={v->model.edit{it.copy(pin=v.filter(Char::isDigit).take(6))}},label={Text("PIN de seis números (primera vez)")},keyboardOptions=KeyboardOptions(keyboardType=KeyboardType.Number),singleLine=true,modifier=Modifier.fillMaxWidth())
            Row(horizontalArrangement=Arrangement.spacedBy(8.dp)){
                Button(onClick={model.discoverWifi()},enabled=!s.busy&&!s.scanning){Text("Buscar Wi-Fi")}
                OutlinedButton(onClick={
                    val needed=if(Build.VERSION.SDK_INT>=31)arrayOf(Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT) else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION,Manifest.permission.ACCESS_COARSE_LOCATION)
                    if(needed.all{context.checkSelfPermission(it)==PackageManager.PERMISSION_GRANTED})model.discoverBle() else permissions(needed)
                },enabled=!s.busy&&!s.scanning){Text("Buscar BLE")}
            }
            if(s.scanning)Text("Buscando MiniPet…")
            s.devices.forEach{device->OutlinedButton(onClick={model.connect(device)},enabled=!s.busy,modifier=Modifier.fillMaxWidth()){Text("${device.title} · ${if(device.bluetooth)"BLE" else device.address}")}}
            HorizontalDivider()
            OutlinedTextField(value=s.address,onValueChange={v->model.edit{it.copy(address=v)}},label={Text("IP local de MiniPet")},supportingText={Text("Ejemplo: 192.168.1.25:8080")},singleLine=true,modifier=Modifier.fillMaxWidth())
            Button(onClick={model.connect(FoundDevice("MiniPet",s.address,false))},enabled=!s.busy,modifier=Modifier.fillMaxWidth()){Text("Conectar por Wi-Fi")}
            TextButton(onClick={context.startActivity(Intent(Settings.ACTION_WIFI_SETTINGS))}){Text("Abrir redes Wi-Fi del teléfono")}
            Text("Usa la misma Wi-Fi. Sin router, conéctate a la red MiniPet cuya clave aparece en la OLED. En BLE, Android puede pedir otra vez el mismo PIN.",style=MaterialTheme.typography.bodySmall)
            Text("En Android 11 y anteriores, la búsqueda BLE requiere permiso de ubicación y ubicación activada. La app no obtiene tu posición.",style=MaterialTheme.typography.bodySmall)
        }else{
            Text("Puedes dejar la mascota a su ritmo, enviarle algo o preparar tu biblioteca.")
            Row(horizontalArrangement=Arrangement.spacedBy(8.dp)){
                Button(onClick={model.edit{it.copy(tab=1)}}){Text("Modo estudio")}
                OutlinedButton(onClick={model.disconnect()},enabled=!s.busy){Text("Desconectar")}
            }
            TextButton(onClick={model.stop()},enabled=!s.busy){Text("Volver a la mascota ahora")}
        }
    }
    Panel("Antes de empezar"){
        Text("Instala el firmware Funciones + apk en el ESP32. El cableado y tus ajustes actuales se conservan. Wi-Fi es la opción más rápida para imágenes y clips; BLE también funciona, con transferencias más lentas.")
    }
}
@OptIn(ExperimentalMaterial3Api::class)
@Composable private fun SendPanel(model: MiniPetModel){
    val s=model.state
    SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()){
        listOf("Texto","Media","Biblioteca").forEachIndexed{i,label->
            SegmentedButton(selected=s.sendTab==i,onClick={model.edit{it.copy(sendTab=i)}},shape=SegmentedButtonDefaults.itemShape(i,3)){Text(label)}
        }
    }
    if(s.study?.active==true)Text("MiniPet está en modo estudio. Puedes preparar o guardar archivos; termina la sesión para mostrarlos.")
    when(s.sendTab){0->TextPanel(model);1->MediaPanel(model);else->LibraryPanel(model)}
}
@Composable private fun rememberLauncherForPermissions(onGranted:()->Unit): (Array<String>)->Unit {
    val launcher=androidx.activity.compose.rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()){result->if(result.values.all{it})onGranted()}
    return {launcher.launch(it)}
}
@Composable private fun TextPanel(model: MiniPetModel){
    val s=model.state
    Panel("Un mensaje para MiniPet"){
        OutlinedTextField(value=s.text,onValueChange={value->if(value.toByteArray().size<=4096)model.edit{it.copy(text=value)}},label={Text("Tu texto")},minLines=3,maxLines=8,modifier=Modifier.fillMaxWidth())
        Text("${s.text.toByteArray().size} / 4096 bytes · acentos y ñ compatibles",style=MaterialTheme.typography.labelSmall)
        Toggle("Desplazamiento horizontal",s.scroll,!s.busy){value->model.edit{it.copy(scroll=value)}}
        if(s.scroll)Control("Velocidad: ${s.speed} píxeles/s",s.speed.toFloat(),10f..80f,enabled=!s.busy){value->model.edit{it.copy(speed=value.toInt())}}
        PixelPreview(listOf(s.text,s.scroll,s.speed)){age->OledText.frame(s.text,age,model.font,s.scroll,s.speed)}
        Text("En páginas se centra en ambos ejes. Los textos largos se leen completos.",style=MaterialTheme.typography.bodySmall)
        Toggle("Mantener hasta que lo quite",s.fixed,!s.busy){value->model.edit{it.copy(fixed=value)}}
        Button(onClick={model.sendText()},enabled=s.connected&&!s.busy&&s.study?.active!=true&&s.text.isNotBlank(),modifier=Modifier.fillMaxWidth()){Text("Mostrar mensaje")}
        OutlinedButton(onClick={model.stop()},enabled=s.connected&&!s.busy,modifier=Modifier.fillMaxWidth()){Text("Volver a MiniPet")}
    }
}
@Composable private fun MediaPanel(model: MiniPetModel){
    val s=model.state
    val picker=androidx.activity.compose.rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()){uri->uri?.let{model.choose(it)}}
    val info=remember(s.prepared){s.prepared?.let{runCatching{MediaFormat.parse(it)}.getOrNull()}}
    Panel("Imágenes y pequeños clips"){
        Button(onClick={picker.launch(arrayOf("image/*","video/*"))},enabled=!s.busy,modifier=Modifier.fillMaxWidth()){Text("Elegir imagen, GIF o vídeo")}
        Text(s.source?.title ?: if(info!=null)"Contenido de tu biblioteca" else "Selecciona un archivo para adaptarlo a la OLED.")
        PixelPreview(s.prepared){age->info?.frame(((age*info.fps/1000)%info.frames).toInt())}
        if(info!=null)Text("${info.frames} cuadros · ${info.fps} fps · ${(s.prepared!!.size+1023)/1024} KiB",style=MaterialTheme.typography.labelSmall)
        if(s.source!=null){
            fun change(next: RenderOptions){model.edit{it.copy(options=next,prepared=null)}}
            Toggle("Ajustar imagen completa",s.options.fit,!s.busy){change(s.options.copy(fit=it))}
            Toggle("Tramado para simular tonos",s.options.dither,!s.busy){change(s.options.copy(dither=it))}
            Toggle("Invertir blanco y negro",s.options.invert,!s.busy){change(s.options.copy(invert=it))}
            Control("Contraste: ${"%.1f".format(s.options.contrast)}",s.options.contrast,.5f..2.5f,enabled=!s.busy){change(s.options.copy(contrast=it))}
            Control("Umbral: ${s.options.threshold}",s.options.threshold.toFloat(),0f..255f,enabled=!s.busy){change(s.options.copy(threshold=it.toInt()))}
            Control("Acercamiento: ${"%.1f".format(s.options.zoom)}×",s.options.zoom,1f..4f,enabled=!s.busy){change(s.options.copy(zoom=it))}
            Control("Encuadre horizontal",s.options.panX,-1f..1f,enabled=!s.busy){change(s.options.copy(panX=it))}
            Control("Encuadre vertical",s.options.panY,-1f..1f,enabled=!s.busy){change(s.options.copy(panY=it))}
            OutlinedButton(onClick={change(s.options.copy(rotation=(s.options.rotation+90)%360))},enabled=!s.busy){Text("Girar 90°")}
            if(s.source.seconds>0){
                if(s.source.seconds>1)Control("Inicio: ${"%.1f".format(s.start)} s",s.start,0f..(s.source.seconds-1).coerceAtLeast(.01f),enabled=!s.busy){value->model.edit{it.copy(start=value,seconds=minOf(it.seconds,(s.source.seconds-value).coerceAtLeast(1f)),prepared=null)}}
                val max=minOf(30f,(s.source.seconds-s.start).coerceAtLeast(1f))
                if(max>1)Control("Duración del clip: ${"%.1f".format(s.seconds)} s",s.seconds,1f..max,enabled=!s.busy){value->model.edit{it.copy(seconds=value,prepared=null)}}
                Control("Cuadros por segundo: ${s.fps}",s.fps.toFloat(),5f..15f,1,!s.busy){value->model.edit{it.copy(fps=when{value<7.5f->5;value<12.5f->10;else->15},prepared=null)}}
            }
            Button(onClick={model.prepare()},enabled=!s.busy,modifier=Modifier.fillMaxWidth()){Text("Preparar vista previa")}
        }
        Text("Hasta 30 s y 15 fps. El teléfono adapta el archivo; no se envía audio. Tras cambiar el encuadre, prepara otra vez la vista previa.",style=MaterialTheme.typography.bodySmall)
        PlaybackControls(model)
        Button(onClick={model.sendPrepared(true)},enabled=s.connected&&!s.busy&&s.study?.active!=true&&info!=null,modifier=Modifier.fillMaxWidth()){Text("Mostrar ahora")}
        OutlinedButton(onClick={model.sendPrepared(false)},enabled=s.connected&&!s.busy&&info!=null,modifier=Modifier.fillMaxWidth()){Text("Guardar en MiniPet sin mostrar")}
        TextButton(onClick={model.saveLocal()},enabled=!s.busy&&info!=null){Text("Guardar en el teléfono")}
        if(s.connected)TextButton(onClick={model.stop()},enabled=!s.busy){Text("Volver a la mascota")}
    }
}
@Composable private fun PlaybackControls(model: MiniPetModel){
    val s=model.state
    Toggle("Mantener hasta que lo quite",s.fixed,!s.busy){value->model.edit{it.copy(fixed=value)}}
    if(!s.fixed)Control("Mostrar durante ${s.duration} segundos",s.duration.toFloat(),3f..120f,enabled=!s.busy){value->model.edit{it.copy(duration=value.toInt())}}
    Toggle("Repetir animación",s.loop,!s.busy){value->model.edit{it.copy(loop=value)}}
    Text("Si la app deja de comunicarse, MiniPet vuelve a su funcionamiento normal en unos 15 s.",style=MaterialTheme.typography.bodySmall)
}
@Composable private fun LibraryPanel(model: MiniPetModel){
    val s=model.state
    Panel("Guardado en el teléfono"){
        if(s.local.isEmpty())Text("Aquí aparecerán los contenidos que prepares y guardes.")
        s.local.forEach{asset->
            Text(asset.title,fontWeight=FontWeight.Bold);Text("${asset.frames} cuadros · ${asset.fps} fps",style=MaterialTheme.typography.labelSmall)
            Row(horizontalArrangement=Arrangement.spacedBy(8.dp)){
                TextButton(onClick={model.previewSaved(asset)},enabled=!s.busy){Text("Ver")}
                TextButton(onClick={model.sendSaved(asset,true)},enabled=s.connected&&!s.busy&&s.study?.active!=true){Text("Mostrar")}
                TextButton(onClick={model.sendSaved(asset,false)},enabled=s.connected&&!s.busy){Text("Guardar en MiniPet")}
            }
            TextButton(onClick={model.deleteLocal(asset.id)},enabled=!s.busy){Text("Eliminar del teléfono")};HorizontalDivider()
        }
    }
    Panel("Guardado en MiniPet"){
        if(!s.connected)Text("Conecta MiniPet para ver su biblioteca.")
        else{
            Text("${s.remote.size} / 8 archivos. Guardar no cambia la pantalla.")
            PlaybackControls(model)
            s.remote.forEach{asset->
                Text(asset.title,fontWeight=FontWeight.Bold)
                Row(horizontalArrangement=Arrangement.spacedBy(8.dp)){
                    Button(onClick={model.playRemote(asset.id)},enabled=!s.busy&&s.study?.active!=true){Text("Mostrar")}
                    TextButton(onClick={model.deleteRemote(asset.id)},enabled=!s.busy){Text("Eliminar de MiniPet")}
                };HorizontalDivider()
            }
            OutlinedButton(onClick={model.refresh()},enabled=!s.busy){Text("Actualizar biblioteca")}
            TextButton(onClick={model.stop()},enabled=!s.busy){Text("Volver a la mascota")}
        }
    }
}
@Composable private fun SettingsPanel(model: MiniPetModel){
    val s=model.state;val settings=remember(s.settings){JSONObject(s.settings)}
    Panel("La personalidad de tu MiniPet"){
        if(!s.connected){Text("Conéctate para leer y cambiar sus ajustes actuales.");return@Panel}
        Text("Los cambios se guardan en el ESP32 y continúan al cerrar la app.")
        listOf("Asistente" to 1,"Reloj" to 2,"Clima" to 4).forEach{(label,mask)->
            val modules=settings.optInt("modules",7)
            Toggle(label,modules and mask!=0,!s.busy){checked->val next=if(checked)modules or mask else modules and mask.inv();if(next!=0)model.setting("modules",next)}
        }
        Control("Humor: ${settings.optInt("humor",50)} %",settings.optInt("humor",50).toFloat(),0f..100f,9,!s.busy){model.setting("humor",(it/10).toInt()*10)}
        Control("Brillo: ${settings.optInt("brightness",120)}",settings.optInt("brightness",120).toFloat(),10f..255f,enabled=!s.busy){model.setting("brightness",it.toInt())}
        listOf("Mensajes" to "messages","Frases aleatorias" to "randomPhrases","Reacciones al clima" to "weatherReactions","Avisos de lluvia" to "weatherNotices","Destello de tormenta" to "stormFlash","Horario No molestar" to "quietEnabled").forEach{(label,key)->Toggle(label,settings.optInt(key)!=0,!s.busy){model.setting(key,if(it)1 else 0)}}
        if(settings.optInt("quietEnabled")!=0){
            Control("No molestar desde: ${settings.optInt("quietStart",22)}:00",settings.optInt("quietStart",22).toFloat(),0f..23f,22,!s.busy){model.setting("quietStart",it.toInt())}
            Control("No molestar hasta: ${settings.optInt("quietEnd",8)}:00",settings.optInt("quietEnd",8).toFloat(),0f..23f,22,!s.busy){model.setting("quietEnd",it.toInt())}
        }
        val names=listOf("Pausada","Normal","Frecuente")
        Text("Frecuencia: ${names[settings.optInt("reminderSpeed",1).coerceIn(0,2)]}")
        Row{names.forEachIndexed{i,name->TextButton(onClick={model.setting("reminderSpeed",i)},enabled=!s.busy){Text(name)}}}
        Text("Vista del clima")
        listOf("Principal","Detalles","Próximos días","Alternar").forEachIndexed{i,label->
            Row(verticalAlignment=Alignment.CenterVertically){RadioButton(selected=settings.optInt("weatherView",3)==i,onClick={model.setting("weatherView",i)},enabled=!s.busy);Text(label)}
        }
        val fields=listOf(Triple("Espera para pedir atención (s)","absenceSeconds",30f..600f),Triple("Dormir tras ausencia (min)","sleepMinutes",5f..60f),Triple("Gesto desde (cm)","gestureMinCm",10f..200f),Triple("Gesto hasta (cm)","gestureMaxCm",20f..300f),Triple("Mantener gesto (ms)","gestureMs",2000f..10000f),Triple("Presencia hasta (cm)","presenceCm",30f..350f),Triple("Tiempo por pantalla (s)","pageSeconds",3f..15f))
        fields.forEach{(label,key,range)->Control("$label: ${settings.optInt(key,range.start.toInt())}",settings.optInt(key,range.start.toInt()).toFloat(),range,enabled=!s.busy){model.setting(key,it.toInt())}}
        Button(onClick={model.saveSettings()},enabled=!s.busy,modifier=Modifier.fillMaxWidth()){Text("Aplicar y guardar ajustes")}
        OutlinedButton(onClick={model.refresh()},enabled=!s.busy,modifier=Modifier.fillMaxWidth()){Text("Leer ajustes de MiniPet")}
    }
    FirmwarePanel(model)
}
