package cl.maxghos.minipet

import android.graphics.Bitmap
import android.graphics.Canvas
import android.view.View
import android.os.SystemClock
import androidx.compose.ui.test.*
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.lifecycle.ViewModelProvider
import org.json.JSONArray
import org.junit.Assert.*
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import org.robolectric.annotation.LooperMode
import java.io.File

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[28],qualifiers="w411dp-h891dp-xhdpi")
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@LooperMode(LooperMode.Mode.PAUSED)
class AppUiTest {
    @get:Rule val compose=createAndroidComposeRule<MainActivity>()
    private fun capture(name: String): Int {
        compose.waitForIdle()
        val target=File("build/previews/$name.png");target.parentFile!!.mkdirs()
        // PixelCopy requires a real window surface. Draw the recomposed view using
        // Robolectric's native graphics after the Compose rule reaches idle.
        compose.runOnIdle{
            val root=compose.activity.window.decorView
            root.measure(View.MeasureSpec.makeMeasureSpec(822,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(1782,View.MeasureSpec.EXACTLY))
            root.layout(0,0,822,1782)
            val bitmap=Bitmap.createBitmap(822,1782,Bitmap.Config.ARGB_8888)
            root.draw(Canvas(bitmap))
            target.outputStream().use{assertTrue(bitmap.compress(Bitmap.CompressFormat.PNG,100,it))}
            bitmap.recycle()
        }
        assertTrue(target.length()>1000)
        return target.readBytes().contentHashCode()
    }
    @Test fun navigatesAllFivePanelsAndBothSendViewsWithoutADevice(){
        val pictures=mutableSetOf<Int>()
        for((index,pair) in listOf("Conectar" to "Vincula tu compañero","Estudio" to "Estudiemos por bloques",
            "Día" to "Tu día, con MiniPet","Enviar" to "Un mensaje para MiniPet","Ajustes" to "La personalidad de tu MiniPet").withIndex()){
            compose.onNodeWithText(pair.first,substring=false).performClick()
            compose.onNodeWithText(pair.second).assertExists()
            pictures+=capture("panel-$index")
        }
        assertEquals(5,pictures.size)
        compose.onNodeWithText("Enviar",substring=false).performClick()
        compose.onNodeWithText("Media",substring=false).performClick()
        compose.onNodeWithText("Imágenes y pequeños clips").assertExists()
        capture("send-media")
        compose.onNodeWithText("Biblioteca",substring=false).performClick()
        compose.onNodeWithText("Guardado en el teléfono").assertExists()
    }
    @Test fun studyFocusBreakPauseWaitingAndSummaryMatchProtocolState(){
        val model=ViewModelProvider(compose.activity)[MiniPetModel::class.java]
        val fixtures=JSONArray(javaClass.getResourceAsStream("/study_cycle.json")!!.bufferedReader().readText())
        for((name,index) in listOf("idle" to 0,"focus" to 1,"break" to 3,"waiting" to 4,"paused" to 5,"long-break" to 7,"summary" to 8)){
            val status=StudyStatus.parse(fixtures.getJSONObject(index).getJSONObject("study"),SystemClock.elapsedRealtime())
            compose.runOnIdle{model.edit{it.copy(tab=1,connected=true,connectionName="MiniPet de prueba",studySupported=true,study=status,studyDraft=StudyDraft.from(status.config))}}
            compose.onNodeWithText(if(status.active)status.label() else "Estudiemos por bloques").assertExists()
            if(status.active)compose.onNodeWithText(if(status.running)"Pausar" else if(status.state=="waiting")"Empezar estudio" else "Reanudar").assertExists()
            else compose.onNodeWithText("Guardar y empezar a estudiar").assertExists()
            if(name=="summary")compose.onNodeWithText("Resumen de la última sesión").assertExists()
            capture("study-$name")
        }
    }
    @Test fun destructiveActionsAskForConfirmationAndStaleStateDisablesControls(){
        val model=ViewModelProvider(compose.activity)[MiniPetModel::class.java]
        val fixtures=JSONArray(javaClass.getResourceAsStream("/study_cycle.json")!!.bufferedReader().readText())
        val status=StudyStatus.parse(fixtures.getJSONObject(1).getJSONObject("study"),SystemClock.elapsedRealtime())
        compose.runOnIdle{model.edit{it.copy(tab=1,connected=true,studySupported=true,study=status,studyDraft=StudyDraft.from(status.config))}}
        compose.onNodeWithText("Pausar").assertIsEnabled()
        compose.onNodeWithText("Saltar etapa").performClick()
        compose.onNodeWithText("¿Saltar esta etapa?").assertExists()
        compose.onNodeWithText("Confirmar salto").assertIsEnabled()
        val next=StudyStatus.parse(fixtures.getJSONObject(3).getJSONObject("study"),SystemClock.elapsedRealtime())
        compose.runOnIdle{model.edit{it.copy(study=next)}}
        compose.onNodeWithText("Confirmar salto").assertIsNotEnabled()
        compose.onNodeWithText("Continuar aquí").performClick()
        compose.runOnIdle{model.edit{it.copy(study=status.copy(receivedAt=SystemClock.elapsedRealtime()))}}
        compose.onNodeWithText("Terminar sesión").performClick()
        compose.onNodeWithText("¿Terminar la sesión?").assertExists()
        compose.onNodeWithText("Continuar aquí").performClick()
        compose.runOnIdle{model.edit{it.copy(study=status.copy(receivedAt=SystemClock.elapsedRealtime()-20000))}}
        compose.onNodeWithText("Pausar").assertIsNotEnabled()
        compose.onNodeWithText("Saltar etapa").assertIsNotEnabled()
        compose.onNodeWithText("Terminar sesión").assertIsNotEnabled()
    }
    @Test fun dayViewsUseCppAgendaAndNeverRevealTheBotToken(){
        val model=ViewModelProvider(compose.activity)[MiniPetModel::class.java]
        val data=org.json.JSONObject(javaClass.getResourceAsStream("/additional.json")!!.bufferedReader().readText())
        compose.runOnIdle{model.edit{it.copy(tab=2,connected=true,additionalSupported=true,agenda=data.getJSONObject("agenda").toString(),telegram=data.getJSONObject("telegram").toString(),dayTab=0)}}
        for((tab,title) in listOf("Actividades" to "Actividades del día","Avisos" to "Crear recordatorio","Resumen" to "Resumen diario","Telegram" to "Telegram personal")){
            compose.onNodeWithText(tab,substring=false).performClick()
            compose.onNodeWithText(title,substring=false).assertExists()
            capture("day-$tab")
        }
        compose.onNodeWithText("Guardar token por BLE").assertIsNotEnabled()
        compose.onNodeWithText("Token de BotFather").assertExists()
        assertFalse(data.getJSONObject("telegram").has("token"));assertFalse(data.getJSONObject("telegram").has("tokenValue"))
    }
    @Test fun backgroundingClearsTheUnsubmittedTelegramToken(){
        val model=ViewModelProvider(compose.activity)[MiniPetModel::class.java]
        compose.runOnIdle{model.edit{it.copy(telegramToken="private-input")};model.foreground(false)}
        assertEquals("",model.state.telegramToken)
        compose.runOnIdle{model.edit{it.copy(connected=true,secureConfiguration=true,telegramToken="private-input")};model.disconnect()}
        assertEquals("",model.state.telegramToken);assertFalse(model.state.secureConfiguration)
    }
    @Test fun telegramUsesTheActiveTransportRatherThanTheSearchSelection(){
        val model=ViewModelProvider(compose.activity)[MiniPetModel::class.java]
        val data=org.json.JSONObject(javaClass.getResourceAsStream("/additional.json")!!.bufferedReader().readText())
        compose.runOnIdle{model.edit{it.copy(tab=2,dayTab=3,connected=true,additionalSupported=true,
            agenda=data.getJSONObject("agenda").toString(),telegram=data.getJSONObject("telegram").toString(),
            bluetooth=false,secureConfiguration=true,telegramToken="private-input")}}
        compose.onNodeWithText("Token de BotFather").assertIsEnabled()
        compose.onNodeWithText("Guardar token por BLE").assertIsEnabled()
        compose.runOnIdle{model.edit{it.copy(bluetooth=true,secureConfiguration=false)}}
        compose.onNodeWithText("Token de BotFather").assertIsNotEnabled()
        compose.onNodeWithText("Guardar token por BLE").assertIsNotEnabled()
    }
}
