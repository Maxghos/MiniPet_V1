package cl.maxghos.minipet

import android.graphics.Bitmap
import android.graphics.Color
import android.net.Uri
import kotlinx.coroutines.runBlocking
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[28],manifest=Config.NONE)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
class ImportTest {
    @Test fun selectedPngConvertsAndSurvivesLibraryRoundTrip(): Unit=runBlocking{
        val context=RuntimeEnvironment.getApplication()
        val source=File(context.cacheDir,"white.png")
        val bitmap=Bitmap.createBitmap(16,4,Bitmap.Config.ARGB_8888)
        bitmap.eraseColor(Color.WHITE)
        source.outputStream().use{assertTrue(bitmap.compress(Bitmap.CompressFormat.PNG,100,it))};bitmap.recycle()
        val importer=MediaImporter(context)
        val document=importer.inspect(Uri.fromFile(source))
        val packed=importer.prepare(document,RenderOptions(dither=false),0f,1f,10){}
        val media=MediaFormat.parse(packed)
        assertEquals(1,media.frames);assertEquals(1,media.fps);assertTrue(media.payload.all{it==(-1).toByte()})
        val library=AssetLibrary(context)
        val saved=library.save("Prueba ñ",packed)
        assertTrue(library.list().any{it.id==saved.id&&it.title=="Prueba ñ"})
        assertArrayEquals(packed,library.read(saved.id))
        library.delete(saved.id);assertFalse(library.list().any{it.id==saved.id})
    }
}
