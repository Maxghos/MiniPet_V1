package cl.maxghos.minipet

import org.junit.Assert.*
import org.junit.Test

class CodecTest {
    private fun lit(bytes:ByteArray,x:Int,y:Int)=(bytes[x+y/8*128].toInt() and (1 shl(y%8)))!=0
    @Test fun whiteAndBlackHaveExactLayout(){
        val white=MonoCodec.convert(IntArray(4096){-1},128,32,RenderOptions(dither=false))
        assertTrue(white.all{it==(-1).toByte()})
        val black=MonoCodec.convert(IntArray(4096){0xff000000.toInt()},128,32,RenderOptions(dither=false))
        assertTrue(black.all{it==0.toByte()})
    }
    @Test fun ssdPagesAndBitOrder(){
        val pixels=IntArray(4096){0xff000000.toInt()};pixels[0]=-1;pixels[31*128+127]=-1
        val frame=MonoCodec.convert(pixels,128,32,RenderOptions(dither=false))
        assertEquals(1,frame[0].toInt());assertEquals(128,frame[511].toInt() and 255)
        assertEquals(2,frame.sumOf{Integer.bitCount(it.toInt() and 255)})
    }
    @Test fun fitDoesNotStretchAndFillCrops(){
        val pixels=IntArray(128*64){-1}
        val fit=MonoCodec.convert(pixels,128,64,RenderOptions(fit=true,dither=false))
        assertFalse(lit(fit,31,16));assertTrue(lit(fit,32,16));assertTrue(lit(fit,95,16));assertFalse(lit(fit,96,16))
        assertTrue(MonoCodec.convert(pixels,128,64,RenderOptions(fit=false)).all{it==(-1).toByte()})
    }
    @Test fun rotationAndTransparency(){
        val frame=MonoCodec.convert(intArrayOf(0xff000000.toInt(),-1),2,1,RenderOptions(rotation=90,dither=false))
        assertFalse(lit(frame,64,15));assertTrue(lit(frame,64,16))
        assertTrue(MonoCodec.convert(IntArray(4096){0x00ffffff},128,32,RenderOptions(dither=false)).all{it==0.toByte()})
    }
    @Test fun orderedDitherHasHalfThePixelsAtMidGray(){
        val frame=MonoCodec.convert(IntArray(4096){0xff808080.toInt()},128,32,RenderOptions())
        assertEquals(2048,frame.sumOf{Integer.bitCount(it.toInt() and 255)})
        val inverse=MonoCodec.convert(IntArray(4096){0xff808080.toInt()},128,32,RenderOptions(invert=true))
        for(i in frame.indices)assertEquals(frame[i].toInt().inv() and 255,inverse[i].toInt() and 255)
    }
    @Test fun binaryRoundTripAndCorruption(){
        val frames=listOf(ByteArray(512){it.toByte()},ByteArray(512){(it*3).toByte()})
        val packed=MediaFormat.pack(frames,10);val media=MediaFormat.parse(packed)
        assertEquals(2,media.frames);assertEquals(10,media.fps);assertArrayEquals(frames[1],media.frame(1))
        packed[30]=(packed[30].toInt() xor 1).toByte()
        assertThrows(IllegalArgumentException::class.java){MediaFormat.parse(packed)}
        assertThrows(IllegalArgumentException::class.java){MediaFormat.pack(List(451){ByteArray(512)},15)}
        assertThrows(IllegalArgumentException::class.java){MediaFormat.pack(listOf(ByteArray(511)),10)}
    }
    @Test fun cppFixtureMatchesTheKotlinEncoder(){
        val fixture=javaClass.getResourceAsStream("/frame_clip.mpet")!!.use{it.readBytes()}
        val frames=List(2){frame->ByteArray(512).also{out->for(y in 0..31)for(x in 0..127)if((x+y+frame*17)%11==0){val i=x+y/8*128;out[i]=(out[i].toInt() or (1 shl(y%8))).toByte()}}}
        assertArrayEquals(fixture,MediaFormat.pack(frames,10))
    }
    @Test fun spanishAndUnsupportedCodepoints(){
        assertArrayEquals(byteArrayOf(160.toByte(),130.toByte(),161.toByte(),162.toByte(),163.toByte(),164.toByte()),OledText.encode("áéíóúñ"))
        assertEquals("Hola?",OledText.encode("Hola😊").toString(Charsets.ISO_8859_1))
        assertEquals(12,OledText.pages("Hola").first().top)
        assertEquals(8,OledText.pages("Hola\nMundo").first().top)
    }
    @Test fun longTextPagesAreBalancedAndComplete(){
        val text="Uno\nDos\nTres\nCuatro\nCinco";val pages=OledText.pages(text)
        assertEquals(listOf(3,2),pages.map{it.lines.size})
        assertEquals(4,pages[0].top);assertEquals(8,pages[1].top)
        val long="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
        assertEquals(long,OledText.pages(long).flatMap{it.lines}.map{it.toString(Charsets.ISO_8859_1)}.joinToString(""))
    }
    @Test fun localAddressesAreValidatedBeforeSendingCredentials(){
        assertEquals("http://192.168.4.1:8080",LocalAddress.normalize("192.168.4.1"))
        assertEquals("http://minipet-abcd.local:8080",LocalAddress.normalize("minipet-abcd.local"))
        for(bad in listOf("example.com","https://192.168.1.1","192.168.1.1@evil.com","fc.evil.com","8.8.8.8","192.168.1.1/path"))
            assertThrows(IllegalArgumentException::class.java){LocalAddress.normalize(bad)}
    }
}
