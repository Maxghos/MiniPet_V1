package cl.maxghos.minipet

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.*
import android.content.Context
import android.os.Build
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.util.UUID

@SuppressLint("MissingPermission")
class BleTransport(private val context: Context, private val device: BluetoothDevice): CommandTransport {
    companion object {
        val SERVICE: UUID = UUID.fromString("899a1000-48a0-4d42-8bd6-602c08a43c7e")
        val COMMAND: UUID = UUID.fromString("899a1001-48a0-4d42-8bd6-602c08a43c7e")
        val RESPONSE: UUID = UUID.fromString("899a1002-48a0-4d42-8bd6-602c08a43c7e")
        val CCC: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }
    override val chunkSize = 120
    override val secureConfiguration = true
    private var gatt: BluetoothGatt? = null
    private var command: BluetoothGattCharacteristic? = null
    private val ready = CompletableDeferred<Unit>()
    private var written: CompletableDeferred<Unit>? = null
    private val notifications = Channel<ByteArray>(256)
    private var mtu = 23
    private var subscribed = false
    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, state: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS && state == BluetoothProfile.STATE_CONNECTED) {
                if (!g.discoverServices()) ready.completeExceptionally(DeviceFailure("No se pudieron leer los servicios BLE"))
            } else if (state == BluetoothProfile.STATE_DISCONNECTED || status != BluetoothGatt.GATT_SUCCESS) {
                val error = DeviceFailure("Se perdió la conexión Bluetooth")
                ready.completeExceptionally(error); written?.completeExceptionally(error); notifications.close(error)
            }
        }
        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val service = g.getService(SERVICE)
            command = service?.getCharacteristic(COMMAND)
            if (status != BluetoothGatt.GATT_SUCCESS || command == null) {
                ready.completeExceptionally(DeviceFailure("Actualiza el firmware de MiniPet para usar la app")); return
            }
            if (!g.requestMtu(247)) subscribe(g)
        }
        override fun onMtuChanged(g: BluetoothGatt, value: Int, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) mtu = value
            subscribe(g)
        }
        override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            if (descriptor.uuid != CCC) return
            if (status == BluetoothGatt.GATT_SUCCESS) ready.complete(Unit)
            else ready.completeExceptionally(DeviceFailure("Vincula Bluetooth usando el PIN de la OLED"))
        }
        override fun onCharacteristicWrite(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) written?.complete(Unit)
            else written?.completeExceptionally(DeviceFailure("No se pudo enviar por Bluetooth"))
        }
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            if (characteristic.uuid == RESPONSE) notifications.trySend(value.copyOf())
        }
        @Deprecated("Compatibility with Android 8–12")
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (Build.VERSION.SDK_INT < 33 && characteristic.uuid == RESPONSE) notifications.trySend(characteristic.value.copyOf())
        }
    }
    private fun subscribe(g: BluetoothGatt) {
        if (subscribed) return
        subscribed = true
        val response = g.getService(SERVICE)?.getCharacteristic(RESPONSE)
        val descriptor = response?.getDescriptor(CCC)
        if (response == null || descriptor == null || !g.setCharacteristicNotification(response,true)) {
            ready.completeExceptionally(DeviceFailure("MiniPet no ofrece respuestas BLE")); return
        }
        val started = if (Build.VERSION.SDK_INT >= 33) g.writeDescriptor(descriptor,BluetoothGattDescriptor.ENABLE_INDICATION_VALUE) == BluetoothStatusCodes.SUCCESS
        else { descriptor.value = BluetoothGattDescriptor.ENABLE_INDICATION_VALUE; g.writeDescriptor(descriptor) }
        if (!started) ready.completeExceptionally(DeviceFailure("No se pudieron activar las respuestas BLE"))
    }
    suspend fun connect() = withContext(Dispatchers.Main) {
        gatt = device.connectGatt(context,false,callback,BluetoothDevice.TRANSPORT_LE)
        withTimeout(60000) { ready.await() }
    }
    override suspend fun exchange(command: JSONObject): JSONObject = withContext(Dispatchers.IO) {
        val link = gatt ?: throw DeviceFailure("Bluetooth no está conectado")
        val characteristic = this@BleTransport.command ?: throw DeviceFailure("Bluetooth no está listo")
        val bytes = (command.toString()+"\n").toByteArray(Charsets.UTF_8)
        val step = minOf(160,mtu-3)
        for (offset in bytes.indices step step) {
            currentCoroutineContext().ensureActive()
            val part = bytes.copyOfRange(offset,minOf(bytes.size,offset+step))
            val gate = CompletableDeferred<Unit>(); written = gate
            val started = if (Build.VERSION.SDK_INT >= 33) link.writeCharacteristic(characteristic,part,BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS
            else { characteristic.writeType=BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT; characteristic.value=part; link.writeCharacteristic(characteristic) }
            if (!started) throw DeviceFailure("Bluetooth está ocupado; vuelve a conectar")
            withTimeout(if(device.bondState==BluetoothDevice.BOND_BONDED)15000 else 60000) { gate.await() }
        }
        withTimeout(20000) {
            val buffer = ByteArrayOutputStream()
            while (true) {
                val part = notifications.receive()
                for (byte in part) {
                    if (byte == '\n'.code.toByte()) {
                        val line = buffer.toString("UTF-8"); buffer.reset()
                        val response = JSONObject(line)
                        if(!response.has("req")&&!response.optBoolean("ok"))throw DeviceFailure(response.optString("error","Respuesta BLE inválida"))
                        if (response.optInt("req") == command.optInt("req")) return@withTimeout response
                    } else {
                        if (buffer.size() >= 65536) throw DeviceFailure("Respuesta BLE demasiado grande")
                        buffer.write(byte.toInt())
                    }
                }
            }
            @Suppress("UNREACHABLE_CODE") JSONObject()
        }
    }
    override fun close() { runCatching{gatt?.disconnect();gatt?.close()};gatt=null;notifications.close() }
}
