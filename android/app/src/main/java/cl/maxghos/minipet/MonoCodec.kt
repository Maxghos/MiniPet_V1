package cl.maxghos.minipet

import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.CRC32
import kotlin.math.*

data class RenderOptions(
    val threshold: Int = 128, val contrast: Float = 1f, val dither: Boolean = true,
    val invert: Boolean = false, val fit: Boolean = false, val zoom: Float = 1f,
    val panX: Float = 0f, val panY: Float = 0f, val rotation: Int = 0
)
data class MediaInfo(val fps: Int, val frames: Int, val payload: ByteArray) {
    fun frame(index: Int): ByteArray = payload.copyOfRange((index % frames) * 512, (index % frames + 1) * 512)
}
object MediaFormat {
    const val MAX_FRAMES = 450
    const val MAX_BYTES = 24 + 512 * MAX_FRAMES
    fun pack(frames: List<ByteArray>, fps: Int): ByteArray {
        require(frames.size in 1..MAX_FRAMES && fps in 1..15 && frames.all { it.size == 512 })
        val payload = ByteArray(frames.size * 512)
        frames.forEachIndexed { index, bytes -> bytes.copyInto(payload, index * 512) }
        return ByteBuffer.allocate(24 + payload.size).order(ByteOrder.LITTLE_ENDIAN).apply {
            put("MPET".toByteArray(Charsets.US_ASCII)); put(1); put(fps.toByte())
            putShort(128); putShort(32); putShort(frames.size.toShort()); putInt(payload.size)
            putInt(CRC32().apply { update(payload) }.value.toInt()); putInt(0); put(payload)
        }.array()
    }
    fun parse(bytes: ByteArray): MediaInfo {
        require(bytes.size in 536..MAX_BYTES) { "Archivo MiniPet fuera de límite" }
        val b = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
        val magic = ByteArray(4).also { b.get(it) }
        require(magic.contentEquals("MPET".toByteArray()) && b.get().toInt() == 1)
        val fps = b.get().toInt() and 255
        require(fps in 1..15 && b.short.toInt() == 128 && b.short.toInt() == 32)
        val count = b.short.toInt() and 65535
        val size = b.int; val crc = b.int.toLong() and 0xffffffffL
        require(b.int == 0 && count in 1..MAX_FRAMES && size == count * 512 && bytes.size == size + 24)
        val payload = bytes.copyOfRange(24, bytes.size)
        require(CRC32().apply { update(payload) }.value == crc) { "El archivo está incompleto" }
        return MediaInfo(fps, count, payload)
    }
}
object MonoCodec {
    private val bayer = intArrayOf(0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5)
    fun convert(pixels: IntArray, width: Int, height: Int, options: RenderOptions): ByteArray {
        require(width > 0 && height > 0 && pixels.size == width * height)
        require(options.threshold in 0..255 && options.contrast in .5f..2.5f && options.zoom in 1f..4f)
        val quarter = ((options.rotation / 90) % 4 + 4) % 4
        val rw = if (quarter % 2 == 0) width else height
        val rh = if (quarter % 2 == 0) height else width
        val scale = (if (options.fit) min(128f/rw, 32f/rh) else max(128f/rw, 32f/rh)) * options.zoom
        val panX = options.panX.coerceIn(-1f, 1f) * max(0f, rw - 128f / scale) / 2
        val panY = options.panY.coerceIn(-1f, 1f) * max(0f, rh - 32f / scale) / 2
        val out = ByteArray(512)
        for (y in 0 until 32) for (x in 0 until 128) {
            val sx = floor((x + .5f - 64) / scale + rw/2f + panX).toInt()
            val sy = floor((y + .5f - 16) / scale + rh/2f + panY).toInt()
            var value = 0f
            if (sx in 0 until rw && sy in 0 until rh) {
                val (ox, oy) = when (quarter) {
                    1 -> sy to height - 1 - sx
                    2 -> width - 1 - sx to height - 1 - sy
                    3 -> width - 1 - sy to sx
                    else -> sx to sy
                }
                val color = pixels[oy * width + ox]
                val alpha = (color ushr 24) / 255f
                value = (.2126f * (color ushr 16 and 255) + .7152f * (color ushr 8 and 255) + .0722f * (color and 255)) * alpha
            }
            value = ((value - 128) * options.contrast + 128).coerceIn(0f, 255f)
            val threshold = options.threshold + if (options.dither) (bayer[(y and 3) * 4 + (x and 3)] - 7.5f) * 8 else 0f
            val lit = (value >= threshold) xor options.invert
            if (lit) { val index = x + (y / 8) * 128; out[index] = (out[index].toInt() or (1 shl (y % 8))).toByte() }
        }
        return out
    }
}
object OledText {
    data class Page(val lines: List<ByteArray>) {
        val top: Int get() = (32 - lines.size * 8) / 2
        val duration: Long get() = (lines.sumOf { it.size } * 80L).coerceIn(3000,7000)
    }
    private val accented = mapOf('á' to 160,'é' to 130,'í' to 161,'ó' to 162,'ú' to 163,'ñ' to 164,'Ñ' to 165,'ü' to 129,'Ü' to 154,'¿' to 168,'¡' to 173,'°' to 248,'É' to 144)
    fun encode(text: String): ByteArray {
        val out = ArrayList<Byte>()
        var offset=0
        while(offset<text.length) {
            val point=Character.codePointAt(text,offset);offset+=Character.charCount(point)
            val c=if(point<=65535)point.toChar() else '?'
            val expansion = when (c) { '“','”' -> "\""; '‘','’' -> "'"; '–','—' -> "-"; '…' -> "..."; '\u00a0' -> " "; 'Á' -> "A"; 'Í' -> "I"; 'Ó' -> "O"; 'Ú' -> "U"; else -> null }
            if (expansion != null) expansion.forEach { out += it.code.toByte() }
            else if (accented.containsKey(c)) out += accented.getValue(c).toByte()
            else if (c == '\n' || c.code in 32..127) out += c.code.toByte()
            else if (c.code >= 128) out += '?'.code.toByte()
        }
        return out.toByteArray()
    }
    fun pages(text: String): List<Page> {
        val encoded = encode(text).toString(Charsets.ISO_8859_1)
        val lines = mutableListOf<String>(); var line = ""; var offset = 0
        fun emit() { lines += line; line = "" }
        while (offset < encoded.length) {
            if (encoded[offset] == '\n') { emit(); offset++; continue }
            if (encoded[offset] == ' ') { offset++; continue }
            var end = offset
            while (end < encoded.length && encoded[end] != ' ' && encoded[end] != '\n') end++
            var word = encoded.substring(offset, end)
            if (line.isNotEmpty() && line.length + 1 + word.length > 21) emit()
            if (line.isNotEmpty()) line += ' '
            while (word.length > 21) {
                val punctuation = word.take(21).indexOfLast { it in "_/-\u002e" }
                val count = if (punctuation >= 6) punctuation + 1 else 21
                line = word.take(count); emit(); word = word.drop(count)
            }
            line += word; offset = end
        }
        if (line.isNotEmpty()) emit()
        if (lines.isEmpty()) lines += ""
        val count = (lines.size + 3) / 4; var position = 0
        return List(count) { p ->
            val length = lines.size / count + if (p < lines.size % count) 1 else 0
            Page(List(length) { lines[position++].toByteArray(Charsets.ISO_8859_1) })
        }
    }
    fun frame(text: String, age: Long, font: ByteArray, scroll: Boolean, speed: Int): ByteArray {
        require(font.size == 1280)
        val out = ByteArray(512)
        fun draw(bytes: ByteArray, x: Int, y: Int) {
            bytes.forEachIndexed { index, byte ->
                val ch = byte.toInt() and 255
                for (dx in 0..4) for (dy in 0..7) {
                    val px = x + index * 6 + dx; val py = y + dy
                    if (px in 0..127 && py in 0..31 && (font[ch * 5 + dx].toInt() and (1 shl dy)) != 0) {
                        val cell = px + (py / 8) * 128; out[cell] = (out[cell].toInt() or (1 shl (py % 8))).toByte()
                    }
                }
            }
        }
        if (scroll) {
            val bytes = encode(text.replace('\n',' '))
            draw(bytes, 128 - ((age * speed / 1000) % (bytes.size * 6 + 128)).toInt(), 12)
        } else {
            val pages = pages(text); var elapsed = age
            val page = pages.firstOrNull { if (elapsed < it.duration) true else { elapsed -= it.duration; false } } ?: pages.last()
            page.lines.forEachIndexed { i, line -> draw(line, (128 - line.size * 6) / 2, page.top + i * 8) }
        }
        return out
    }
}
