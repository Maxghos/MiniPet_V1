package cl.maxghos.minipet

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Movie
import android.media.MediaMetadataRetriever
import android.net.Uri
import android.provider.OpenableColumns
import androidx.exifinterface.media.ExifInterface
import kotlinx.coroutines.*
import java.io.File
import java.util.UUID
import org.json.JSONArray
import org.json.JSONObject

// All decoding and conversion runs off the UI thread. Only selected documents are read.
class MediaImporter(private val context: Context) {
    data class Source(val uri: Uri,val title: String,val type: String,val seconds: Float)
    suspend fun inspect(uri: Uri): Source = withContext(Dispatchers.IO) {
        val resolver=context.contentResolver
        val name=resolver.query(uri,arrayOf(OpenableColumns.DISPLAY_NAME),null,null,null)?.use { c -> if(c.moveToFirst()) c.getString(0) else null } ?: "Contenido"
        val mime=resolver.getType(uri) ?: "image/*"
        val seconds=if(mime.startsWith("video/")) retriever(uri) { (it.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)?.toLongOrNull() ?: 0)/1000f }
        else if(mime=="image/gif") {
            val movie=resolver.openInputStream(uri)?.use { Movie.decodeStream(it) }
            (movie?.duration()?.takeIf { it>0 } ?: 1000)/1000f
        } else 0f
        Source(uri,name.take(32),mime,seconds)
    }
    private fun <T> retriever(uri: Uri, action: (MediaMetadataRetriever)->T): T {
        val r=MediaMetadataRetriever()
        try { context.contentResolver.openFileDescriptor(uri,"r").use { p -> requireNotNull(p);r.setDataSource(p.fileDescriptor);return action(r) } }
        finally { r.release() }
    }
    suspend fun prepare(source: Source,options: RenderOptions,start: Float,seconds: Float,fps: Int,progress: (Float)->Unit): ByteArray = withContext(Dispatchers.Default) {
        require(fps in 1..15 && seconds in 1f..30f && start>=0)
        val frames=mutableListOf<ByteArray>()
        fun convert(bitmap: Bitmap, extraRotation: Int=0): ByteArray {
            val pixels=IntArray(bitmap.width*bitmap.height)
            bitmap.getPixels(pixels,0,bitmap.width,0,0,bitmap.width,bitmap.height)
            return MonoCodec.convert(pixels,bitmap.width,bitmap.height,options.copy(rotation=options.rotation+extraRotation))
        }
        if(source.type.startsWith("video/")) {
            val duration=minOf(seconds,(source.seconds-start).coerceAtLeast(0f)); require(duration>0) { "El inicio está fuera del vídeo" }
            val count=(duration*fps).toInt().coerceIn(1,450)
            retriever(source.uri) { r ->
                val width=r.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH)?.toIntOrNull() ?: 0
                val height=r.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT)?.toIntOrNull() ?: 0
                require(width>0 && height>0 && width.toLong()*height<=33_554_432) { "Vídeo demasiado grande" }
                repeat(count) { n ->
                    ensureActive()
                    val time=((start+n.toFloat()/fps)*1_000_000).toLong()
                    val bitmap=if(android.os.Build.VERSION.SDK_INT>=27) r.getScaledFrameAtTime(time,MediaMetadataRetriever.OPTION_CLOSEST,512,512)
                        else r.getFrameAtTime(time,MediaMetadataRetriever.OPTION_CLOSEST)
                    requireNotNull(bitmap) { "Android no pudo decodificar este vídeo" }
                    try { frames+=convert(bitmap) } finally { bitmap.recycle() }
                    progress((n+1f)/count)
                }
            }
        } else if(source.type=="image/gif") {
            val data=context.contentResolver.openInputStream(source.uri)?.use { it.readBytesLimited(8*1024*1024) } ?: error("No se pudo abrir GIF")
            val movie=Movie.decodeByteArray(data,0,data.size) ?: error("GIF incompatible")
            require(movie.width()>0 && movie.height()>0 && movie.width().toLong()*movie.height()<=4_194_304) { "GIF demasiado grande" }
            val duration=minOf(seconds,(source.seconds-start).coerceAtLeast(0f));require(duration>0)
            val count=(duration*fps).toInt().coerceIn(1,450)
            val bitmap=Bitmap.createBitmap(movie.width(),movie.height(),Bitmap.Config.ARGB_8888);val canvas=Canvas(bitmap)
            try { repeat(count) { n ->
                ensureActive();bitmap.eraseColor(android.graphics.Color.TRANSPARENT)
                movie.setTime(((start+n.toFloat()/fps)*1000).toInt());movie.draw(canvas,0f,0f)
                frames+=convert(bitmap);progress((n+1f)/count)
            } } finally { bitmap.recycle() }
        } else {
            val bounds=BitmapFactory.Options().apply { inJustDecodeBounds=true }
            context.contentResolver.openInputStream(source.uri)?.use { BitmapFactory.decodeStream(it,null,bounds) }
            require(bounds.outWidth>0 && bounds.outHeight>0) { "Imagen incompatible" }
            var sample=1;while(bounds.outWidth/sample>1024 || bounds.outHeight/sample>1024)sample*=2
            val bitmap=context.contentResolver.openInputStream(source.uri)?.use { BitmapFactory.decodeStream(it,null,BitmapFactory.Options().apply { inSampleSize=sample }) }
            requireNotNull(bitmap)
            val rotation=runCatching { context.contentResolver.openInputStream(source.uri)!!.use { ExifInterface(it).rotationDegrees } }.getOrDefault(0)
            try { frames+=convert(bitmap,rotation) } finally { bitmap.recycle() };progress(1f)
        }
        MediaFormat.pack(frames,if(frames.size==1)1 else fps)
    }
}
fun java.io.InputStream.readBytesLimited(limit: Int): ByteArray {
    val out=java.io.ByteArrayOutputStream();val block=ByteArray(8192)
    while(true){ val n=read(block);if(n<0)break;require(out.size()+n<=limit){"Archivo demasiado grande"};out.write(block,0,n) }
    return out.toByteArray()
}
data class LocalAsset(val id: String,val title: String,val bytes: Int,val frames: Int,val fps: Int)
class AssetLibrary(context: Context) {
    private val root=File(context.filesDir,"media").apply { mkdirs() }
    fun list(): List<LocalAsset> = root.listFiles()?.filter { it.extension=="json" }?.mapNotNull { file ->
        runCatching { val j=JSONObject(file.readText());val id=j.getString("id");require(id.matches(Regex("[a-zA-Z0-9_-]{1,24}")));LocalAsset(id,j.getString("title"),j.getInt("bytes"),j.getInt("frames"),j.getInt("fps")) }.getOrNull()
    }?.sortedBy { it.title.lowercase() } ?: emptyList()
    fun save(title: String,bytes: ByteArray): LocalAsset {
        require(list().size<100) { "La biblioteca del teléfono llegó a 100 archivos" }
        val media=MediaFormat.parse(bytes);val id=UUID.randomUUID().toString().replace("-", "").take(16)
        val clean=title.trim().take(24).ifBlank { "Contenido" }
        File(root,"$id.part").writeBytes(bytes)
        check(File(root,"$id.part").renameTo(File(root,"$id.mpet")))
        File(root,"$id.json").writeText(JSONObject().put("id",id).put("title",clean).put("bytes",bytes.size).put("frames",media.frames).put("fps",media.fps).toString())
        return LocalAsset(id,clean,bytes.size,media.frames,media.fps)
    }
    fun read(id: String): ByteArray { require(id.matches(Regex("[a-zA-Z0-9_-]{1,24}")));return File(root,"$id.mpet").readBytes().also { MediaFormat.parse(it) } }
    fun delete(id: String) { require(id.matches(Regex("[a-zA-Z0-9_-]{1,24}")));File(root,"$id.mpet").delete();File(root,"$id.json").delete() }
}
