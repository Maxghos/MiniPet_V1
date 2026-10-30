package cl.maxghos.minipet

import android.graphics.Bitmap
import android.graphics.Canvas
import android.os.Looper
import android.view.View
import androidx.lifecycle.ViewModelProvider
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import org.robolectric.annotation.LooperMode
import java.io.File
import java.time.Duration

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[28],qualifiers="w411dp-h891dp-xhdpi")
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@LooperMode(LooperMode.Mode.PAUSED)
class AppUiTest {
    @Test fun launchesAndDrawsAllFivePanelsWithoutADevice(){
        val controller=Robolectric.buildActivity(MainActivity::class.java).setup().visible()
        val activity=controller.get()
        val model=ViewModelProvider(activity)[MiniPetModel::class.java]
        val root=activity.window.decorView
        val out=File("build/previews").apply{mkdirs()}
        for(tab in 0..4){
            model.edit{it.copy(tab=tab)}
            shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(200))
            root.measure(View.MeasureSpec.makeMeasureSpec(822,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(1782,View.MeasureSpec.EXACTLY))
            root.layout(0,0,822,1782)
            val bitmap=Bitmap.createBitmap(822,1782,Bitmap.Config.ARGB_8888)
            root.draw(Canvas(bitmap))
            File(out,"panel-$tab.png").outputStream().use{assertTrue(bitmap.compress(Bitmap.CompressFormat.PNG,100,it))}
            assertTrue(File(out,"panel-$tab.png").length()>1000)
            bitmap.recycle()
        }
        assertFalse(activity.isFinishing)
        controller.pause().stop().destroy()
    }
}
