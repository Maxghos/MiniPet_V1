package cl.maxghos.minipet

import android.annotation.SuppressLint
import android.app.Application
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.net.Uri
import android.os.ParcelUuid
import android.os.SystemClock
import androidx.compose.runtime.*
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.*
import org.json.JSONObject

data class FoundDevice(val title: String,val address: String,val bluetooth: Boolean)
data class DeviceAsset(val id: String,val title: String,val frames: Int,val fps: Int)
data class AppState(
    val tab: Int=0,val sendTab: Int=0,val address: String="192.168.4.1:8080",val pin: String="",
    val bluetooth: Boolean=false,val devices: List<FoundDevice> = emptyList(),
    val connected: Boolean=false,val connectionName: String="",val busy: Boolean=false,
    val scanning: Boolean=false,val progress: Float=0f,val notice: String="",val error: Boolean=false,
    val source: MediaImporter.Source?=null,val options: RenderOptions=RenderOptions(),
    val start: Float=0f,val seconds: Float=5f,val fps: Int=10,val prepared: ByteArray?=null,val preparedId: String?=null,
    val local: List<LocalAsset> = emptyList(),val remote: List<DeviceAsset> = emptyList(),
    val settings: String="{}",val text: String="Hola, MiniPet",val scroll: Boolean=false,
    val speed: Int=25,val duration: Int=10,val fixed: Boolean=false,val loop: Boolean=true,
    val study: StudyStatus?=null,val studySupported: Boolean=false,
    val studyDraft: StudyDraft=StudyDraft(),val studyDirty: Boolean=false,
    val additionalSupported: Boolean=false,val dayTab: Int=0,val agenda: String="{}",val telegram: String="{}",
    val taskTitle: String="",val taskDay: String="",val reminderTitle: String="",val reminderDay: String="",
    val reminderTime: String="18:00",val reminderDaily: Boolean=false,val telegramToken: String="",
    val summaryTime: String="20:30",val dailySummary: Boolean=true,val scenes: Boolean=true,val dayOptionsDirty: Boolean=false
)
class MiniPetModel(application: Application): AndroidViewModel(application) {
    var state by mutableStateOf(AppState());private set
    private val context=getApplication<Application>()
    private val vault=LinkVault(context)
    val font: ByteArray=context.assets.open("oled_font.bin").use{it.readBytes()}
    private val library=AssetLibrary(context)
    private val importer=MediaImporter(context)
    private var client: DeviceClient?=null
    private var job: Job?=null
    private var heartbeat: Job?=null
    private var foreground=true
    private var discovery: WifiDiscovery?=null
    private var bleScan: ScanCallback?=null
    private var lastDaySync=0L
    init{state=state.copy(address=vault.address,local=library.list())}
    fun edit(change: (AppState)->AppState){state=change(state)}
    private fun announce(message: String,error: Boolean=false){state=state.copy(notice=message,error=error)}
    private fun work(label: String,action: suspend ()->Unit){
        if(state.busy)return
        job=viewModelScope.launch {
            state=state.copy(busy=true,progress=0f,notice=label,error=false)
            try{action()}
            catch(_:CancellationException){announce("Operación cancelada")}
            catch(error:Exception){announce(error.message ?: "No se pudo completar la operación",true)}
            catch(_:OutOfMemoryError){announce("El archivo es demasiado grande para este teléfono",true)}
            finally{state=state.copy(busy=false,progress=0f)}
        }
    }
    fun cancel(){job?.cancel()}
    fun foreground(value: Boolean){
        foreground=value
        if(!value)state=state.copy(telegramToken="")
        if(value&&state.connected&&state.studySupported)viewModelScope.launch{runCatching{refreshStudy()}}
    }
    private fun linked(): DeviceClient=client ?: throw DeviceFailure("Conecta MiniPet primero")
    fun connect(device: FoundDevice?=null)=work("Conectando con MiniPet…"){
        stopDiscovery()
        val selected=device ?: FoundDevice("MiniPet",state.address,state.bluetooth)
        val endpoint=if(selected.bluetooth)"ble:${selected.address}" else LocalAddress.normalize(selected.address)
        val transport=if(selected.bluetooth){
            val manager=context.getSystemService(BluetoothManager::class.java)
            @SuppressLint("MissingPermission") val remote=manager.adapter.getRemoteDevice(selected.address)
            BleTransport(context,remote).also{it.connect()}
        }else HttpTransport(endpoint){wifiConnection(context,it)}
        val candidate=DeviceClient(transport,vault.load(endpoint))
        try{
            val pin=state.pin
            if(pin.isNotBlank())vault.save(endpoint,candidate.pair(pin))
            val response=candidate.call("status")
            client?.close();heartbeat?.cancel();client=candidate
            if(!selected.bluetooth)vault.address=endpoint
            state=state.copy(connected=true,connectionName=if(selected.bluetooth)"Bluetooth BLE" else endpoint,
                settings=response.getJSONObject("settings").toString(),pin="",bluetooth=selected.bluetooth,
                studySupported=response.has("study"),study=null,studyDirty=false,
                additionalSupported=response.optBoolean("additional"),agenda="{}",telegram="{}",dayOptionsDirty=false,telegramToken="")
            response.optJSONObject("study")?.let{acceptStudy(StudyStatus.parse(it,SystemClock.elapsedRealtime()))}
            heartbeat=viewModelScope.launch {
                while(isActive){
                    delay(if(state.studySupported)2000 else 5000)
                    if(foreground)try{
                        if(state.studySupported)refreshStudy() else candidate.call("ping")
                        if(state.additionalSupported&&state.tab==2&&!state.busy&&SystemClock.elapsedRealtime()-lastDaySync>=15000){
                            try{refreshAdditional()}catch(error:CancellationException){throw error}catch(_:Exception){announce("La agenda no pudo sincronizarse. Puedes reintentar sin detener el estudio.",true)}
                        }
                    }catch(error:CancellationException){throw error}
                    catch(_:Exception){
                        if(client===candidate){client=null;candidate.close();state=state.copy(connected=false);announce("Conexión perdida. MiniPet continúa por su cuenta.",true)}
                        break
                    }
                }
            }
            if(response.optBoolean("storage"))refreshRemote()
            announce(if(response.optBoolean("storage"))"Conectado. MiniPet sigue a su ritmo." else "Conectado. Texto y ajustes disponibles; revisa la biblioteca en el dispositivo.")
        }catch(error:Exception){candidate.close();throw error}
    }
    fun disconnect(){heartbeat?.cancel();client?.close();client=null;state=state.copy(connected=false);announce("Desconectado. MiniPet continúa por su cuenta.")}
    private fun acceptStudy(value: StudyStatus){
        val previous=state.study
        state=state.copy(study=value,studyDraft=if(!state.studyDirty||value.active)StudyDraft.from(value.config) else state.studyDraft,
            studyDirty=state.studyDirty&&!value.active)
        if(previous!=null&&previous.eventId!=value.eventId&&value.eventAgeMs<10000){
            when(value.event){
                "focusDone"->announce(if(value.phase=="longBreak")"Ciclo completado. Te toca un descanso largo." else "Bloque completado. Es momento de descansar.")
                "breakDone"->announce(if(value.state=="waiting")"Terminó el descanso. Continúa cuando estés listo." else "Terminó el descanso. Volvemos al estudio.")
                "recovered"->announce("Sesión recuperada y pausada. Continúa cuando estés listo.")
            }
        }
    }
    private suspend fun refreshStudy(){
        val current=linked();val value=StudyClient(current).get(SystemClock::elapsedRealtime)
        if(client===current)acceptStudy(value)
    }
    fun syncStudy()=work("Sincronizando estudio…"){refreshStudy();announce("Estado de estudio actualizado")}
    fun editStudy(change: (StudyDraft)->StudyDraft){state=state.copy(studyDraft=change(state.studyDraft),studyDirty=true)}
    fun studyPreset(config: StudyConfig){editStudy{StudyDraft.from(config)}}
    fun studyAction(action: String,expectedRevision: Long?=null)=work("Actualizando la sesión…"){
        val current=state.study ?: throw DeviceFailure("Actualiza el firmware y sincroniza MiniPet primero")
        val config=if(action=="start"||action=="configure")state.studyDraft.config()
            ?: throw DeviceFailure("Revisa los minutos y el número de bloques") else null
        try{
            val value=StudyClient(linked()).action(action,expectedRevision?:current.revision,config,SystemClock::elapsedRealtime)
            if(config!=null)state=state.copy(studyDirty=false)
            acceptStudy(value)
            announce(when(action){"start"->"Vamos a estudiar. MiniPet se encarga del tiempo.";"pause"->"Sesión pausada. Tómate el tiempo que necesites."
                "resume"->"Continuamos con la etapa pendiente.";"skip"->"Etapa saltada. El bloque incompleto no suma como completado."
                "stop"->"Sesión terminada. Aquí queda tu resumen.";else->"Programa guardado en MiniPet."})
            if(!value.persisted)announce("La sesión está disponible, pero no se pudo guardar para un reinicio.",true)
        }catch(error:Exception){
            withContext(NonCancellable){runCatching{refreshStudy()}}
            throw error
        }
    }
    fun sendText()=work("Enviando mensaje…"){
        require(state.text.isNotBlank()&&state.text.toByteArray().size<=4096){"Usa entre 1 y 4096 bytes de texto"}
        linked().call("text",JSONObject().put("text",state.text).put("scroll",state.scroll).put("speed",state.speed)
            .put("durationMs",if(state.fixed)0 else -1))
        announce("Mensaje en pantalla. Al terminar vuelve a MiniPet.")
    }
    fun stop()=work("Volviendo a la mascota…"){linked().call("stop");announce("MiniPet recuperó su pantalla habitual")}
    fun choose(uri: Uri)=work("Abriendo archivo…"){
        val source=importer.inspect(uri)
        state=state.copy(source=source,start=0f,seconds=minOf(5f,source.seconds.coerceAtLeast(1f)),prepared=null,tab=3,sendTab=1)
        prepareNow()
    }
    fun prepare()=work("Adaptando a 128 × 32…"){prepareNow()}
    private suspend fun prepareNow(){
        val source=state.source ?: throw DeviceFailure("Selecciona una imagen, GIF o vídeo")
        val options=state.options;val start=state.start;val seconds=state.seconds;val fps=state.fps
        val data=importer.prepare(source,options,start,seconds,fps){progress->viewModelScope.launch{state=state.copy(progress=progress)}}
        state=state.copy(prepared=data,preparedId=null);announce("Vista previa lista: ${MediaFormat.parse(data).frames} cuadros")
    }
    private suspend fun ensureLocal(): LocalAsset {
        val data=state.prepared ?: throw DeviceFailure("Prepara el contenido primero")
        state.local.firstOrNull{it.id==state.preparedId}?.let{return it}
        val saved=withContext(Dispatchers.IO){library.save(state.source?.title ?: "Contenido",data)}
        state=state.copy(local=library.list(),preparedId=saved.id);return saved
    }
    fun saveLocal()=work("Guardando en el teléfono…"){ensureLocal();announce("Guardado en la biblioteca del teléfono")}
    private suspend fun transfer(id: String,title: String,bytes: ByteArray,show: Boolean){
        val c=linked();val fixed=state.fixed;val duration=state.duration;val loop=state.loop
        c.upload(id,title,bytes){progress->viewModelScope.launch{state=state.copy(progress=progress)}}
        if(show)c.call("play",JSONObject().put("id",id).put("durationMs",if(fixed)0 else duration*1000).put("loop",loop))
        refreshRemote();announce(if(show)"Contenido en pantalla" else "Guardado en MiniPet sin cambiar su pantalla")
    }
    fun sendPrepared(show: Boolean)=work(if(show)"Enviando para mostrar…" else "Guardando en MiniPet…"){
        val data=state.prepared ?: throw DeviceFailure("Prepara el contenido primero")
        val saved=ensureLocal();transfer(saved.id,saved.title,data,show)
    }
    fun sendSaved(asset: LocalAsset,show: Boolean)=work("Enviando ${asset.title}…"){
        val data=withContext(Dispatchers.IO){library.read(asset.id)};state=state.copy(prepared=data,preparedId=asset.id)
        transfer(asset.id,asset.title,data,show)
    }
    fun previewSaved(asset: LocalAsset)=work("Abriendo ${asset.title}…"){
        state=state.copy(prepared=withContext(Dispatchers.IO){library.read(asset.id)},preparedId=asset.id,source=null,tab=3,sendTab=1)
        announce("Vista previa de ${asset.title}")
    }
    fun deleteLocal(id: String)=work("Eliminando del teléfono…"){
        withContext(Dispatchers.IO){library.delete(id)};state=state.copy(local=library.list(),preparedId=state.preparedId.takeUnless{it==id});announce("Eliminado del teléfono")
    }
    fun playRemote(id: String)=work("Mostrando contenido…"){
        linked().call("play",JSONObject().put("id",id).put("durationMs",if(state.fixed)0 else state.duration*1000).put("loop",state.loop));announce("Contenido en pantalla")
    }
    fun deleteRemote(id: String)=work("Eliminando de MiniPet…"){
        linked().call("delete",JSONObject().put("id",id));refreshRemote();announce("Eliminado de MiniPet")
    }
    private suspend fun refreshRemote(){
        val items=linked().call("library").getJSONArray("items")
        state=state.copy(remote=List(items.length()){i->val item=items.getJSONObject(i);DeviceAsset(item.getString("id"),item.optString("title","Contenido"),item.optInt("frames",1),item.optInt("fps",1))})
    }
    fun refresh()=work("Actualizando…"){
        val data=linked().call("status");state=state.copy(settings=data.getJSONObject("settings").toString());refreshRemote();announce("Actualizado")
    }
    fun setting(name: String,value: Int){state=state.copy(settings=JSONObject(state.settings).put(name,value).toString())}
    fun saveSettings()=work("Aplicando ajustes…"){
        linked().call("settings.set",JSONObject().put("settings",JSONObject(state.settings)))
        delay(150);state=state.copy(settings=linked().call("settings.get").getJSONObject("settings").toString());announce("Ajustes enviados y sincronizados")
    }
    private fun acceptAdditional(reply: JSONObject){
        reply.optJSONObject("agenda")?.let{agenda->
            val options=agenda.getJSONObject("options")
            state=state.copy(agenda=agenda.toString(),
                summaryTime=if(state.dayOptionsDirty)state.summaryTime else clockMinute(options.optInt("summaryMinute",1230)),
                dailySummary=if(state.dayOptionsDirty)state.dailySummary else options.optBoolean("dailySummary",true),
                scenes=if(state.dayOptionsDirty)state.scenes else options.optBoolean("scenes",true))
        }
        reply.optJSONObject("telegram")?.let{state=state.copy(telegram=it.toString())}
    }
    private suspend fun refreshAdditional(){
        lastDaySync=SystemClock.elapsedRealtime()
        val current=linked()
        val agenda=current.call("agenda.get");if(client!==current)return;acceptAdditional(agenda)
        val telegram=current.call("telegram.get");if(client!==current)return;acceptAdditional(telegram)
        lastDaySync=SystemClock.elapsedRealtime()
    }
    fun loadDay()=work("Sincronizando agenda y Telegram…"){
        require(state.additionalSupported){"Actualiza el firmware a MiniPet 3.2"};refreshAdditional();announce("Agenda sincronizada")
    }
    fun dayCommand(op: String,fields: JSONObject=JSONObject())=work("Actualizando agenda…"){
        try{
            val reply=linked().call(op,fields);acceptAdditional(reply)
            if(op=="task.add")state=state.copy(taskTitle="")
            if(op=="reminder.add")state=state.copy(reminderTitle="")
            announce(if(reply.optJSONObject("agenda")?.optBoolean("persisted",true)==false)"Cambio aplicado, pero no pudo guardarse tras un reinicio." else "Agenda actualizada",reply.optJSONObject("agenda")?.optBoolean("persisted",true)==false)
        }catch(error:Exception){withContext(NonCancellable){runCatching{refreshAdditional()}};throw error}
    }
    fun addTask(){
        val fields=JSONObject().put("title",state.taskTitle.trim())
        if(state.taskDay.isNotBlank())fields.put("day",state.taskDay.toIntOrNull()?:-1)
        dayCommand("task.add",fields)
    }
    fun addReminder(){
        val minute=parseClockMinute(state.reminderTime)?:-1
        val fields=JSONObject().put("title",state.reminderTitle.trim()).put("minute",minute).put("daily",state.reminderDaily)
        if(state.reminderDay.isNotBlank())fields.put("day",state.reminderDay.toIntOrNull()?:-1)
        dayCommand("reminder.add",fields)
    }
    fun editDayOptions(change:(AppState)->AppState){state=change(state).copy(dayOptionsDirty=true)}
    fun saveDayOptions()=work("Guardando opciones del día…"){
        val minute=parseClockMinute(state.summaryTime)?:throw DeviceFailure("Usa una hora HH:MM válida")
        val reply=linked().call("agenda.options",JSONObject().put("summaryMinute",minute).put("dailySummary",state.dailySummary).put("scenes",state.scenes))
        state=state.copy(dayOptionsDirty=false);acceptAdditional(reply);announce("Opciones guardadas")
    }
    fun configureTelegram()=work("Guardando Telegram por BLE…"){
        require(state.bluetooth){"Para guardar el token, conecta MiniPet por BLE cifrado"}
        val token=state.telegramToken.trim();state=state.copy(telegramToken="")
        require(token.length in 32..100){"Revisa el token de BotFather"}
        acceptAdditional(AdditionalClient(linked()).configureTelegram(token))
        announce("Token guardado. Conecta MiniPet a internet y abre Vincular Telegram en la OLED.")
    }
    fun telegramFlag(key: String,value: Boolean)=work("Guardando avisos de Telegram…"){
        require(key in listOf("enabled","studyAlerts","reminderAlerts","summaryAlerts"))
        acceptAdditional(linked().call("telegram.settings",JSONObject().put(key,value)));announce("Opciones de Telegram guardadas")
    }
    fun clearTelegram()=work("Desvinculando Telegram…"){
        acceptAdditional(linked().call("telegram.clear"));state=state.copy(telegramToken="");announce("Token y vinculación borrados del ESP32")
    }
    fun discoverWifi(){
        stopDiscovery();state=state.copy(devices=emptyList(),scanning=true,bluetooth=false)
        discovery=WifiDiscovery(context){title,address->viewModelScope.launch{found(FoundDevice(title,address,false))}}.also{it.start()}
        viewModelScope.launch{delay(10000);stopDiscovery()}
    }
    private fun found(device: FoundDevice){if(state.devices.none{it.address==device.address})state=state.copy(devices=state.devices+device)}
    @SuppressLint("MissingPermission")
    fun discoverBle(){
        stopDiscovery();state=state.copy(devices=emptyList(),scanning=true,bluetooth=true)
        val adapter=context.getSystemService(BluetoothManager::class.java)?.adapter
        if(adapter==null||!adapter.isEnabled){state=state.copy(scanning=false);announce("Activa Bluetooth en el teléfono",true);return}
        val callback=object:ScanCallback(){
            override fun onScanResult(type:Int,result:ScanResult){viewModelScope.launch{found(FoundDevice(result.device.name ?: "MiniPet",result.device.address,true))}}
            override fun onScanFailed(code:Int){viewModelScope.launch{stopDiscovery();announce("No se pudo buscar por Bluetooth",true)}}
        }
        bleScan=callback
        try{adapter.bluetoothLeScanner.startScan(listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(BleTransport.SERVICE)).build()),ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),callback)}
        catch(_:SecurityException){stopDiscovery();announce("Autoriza el permiso de dispositivos cercanos",true)}
        viewModelScope.launch{delay(10000);stopDiscovery()}
    }
    @SuppressLint("MissingPermission")
    private fun stopDiscovery(){discovery?.close();discovery=null;bleScan?.let{callback->runCatching{context.getSystemService(BluetoothManager::class.java)?.adapter?.bluetoothLeScanner?.stopScan(callback)}};bleScan=null;state=state.copy(scanning=false)}
    override fun onCleared(){stopDiscovery();client?.close();super.onCleared()}
}
