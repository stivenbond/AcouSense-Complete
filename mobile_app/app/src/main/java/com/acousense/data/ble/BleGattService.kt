package com.acousense.data.ble

import android.annotation.SuppressLint
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattServer
import android.bluetooth.BluetoothGattServerCallback
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothManager
import android.bluetooth.le.AdvertiseCallback
import android.bluetooth.le.AdvertiseData
import android.bluetooth.le.AdvertiseSettings
import android.bluetooth.le.BluetoothLeAdvertiser
import android.content.Context
import android.content.Intent
import android.os.IBinder
import android.util.Log
import androidx.core.app.NotificationCompat
import com.acousense.domain.BleRepository
import com.acousense.domain.ExposureRepository
import dagger.hilt.android.AndroidEntryPoint
import java.util.UUID
import javax.inject.Inject
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch

@AndroidEntryPoint
class BleGattService : Service() {

    companion object {
        private const val TAG = "BleGattService"
        const val ACTION_BLE_UNAVAILABLE = "com.acousense.BLE_UNAVAILABLE"
    }

    @Inject lateinit var exposureRepository: ExposureRepository
    @Inject lateinit var bleRepository: BleRepository

    private val serviceScope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private var gattServer: BluetoothGattServer? = null
    private var advertiser: BluetoothLeAdvertiser? = null

    private val gattCallback = object : BluetoothGattServerCallback() {
        override fun onCharacteristicWriteRequest(
            device: BluetoothDevice?,
            requestId: Int,
            characteristic: BluetoothGattCharacteristic?,
            preparedWrite: Boolean,
            responseNeeded: Boolean,
            offset: Int,
            value: ByteArray?
        ) {
            val payloadBytes = value ?: ByteArray(0)
            val parseResult = SyncPayloadParser.parse(payloadBytes)
            if (parseResult.isSuccess) {
                val payload = parseResult.getOrThrow()
                serviceScope.launch {
                    exposureRepository.insertSession(
                        com.acousense.data.db.SyncSession(
                            userIdentifier  = payload.userIdentifier,
                            periodStart     = payload.periodStart,
                            periodEnd       = payload.periodEnd,
                            avgLevel        = payload.avgLevel,
                            maxLevel        = payload.maxLevel,
                            peakCount       = payload.peakCount,
                            exposureSeconds = payload.exposureSeconds,
                            classification  = payload.classification
                        )
                    )
                    bleRepository.emit(payload)
                }
                if (responseNeeded) {
                    gattServer?.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, null)
                }
            } else if (responseNeeded) {
                gattServer?.sendResponse(device, requestId, BluetoothGatt.GATT_FAILURE, offset, null)
            }
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        try {
            createNotificationChannel()
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.Q) {
                startForeground(1001, createNotification(), android.content.pm.ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
            } else {
                startForeground(1001, createNotification())
            }
        } catch (e: Exception) {
            Log.e(TAG, "startForeground failed: ${e.message}", e)
            bleRepository.setGattRunning(false)
            try { stopSelf() } catch (_: Exception) { }
            return
        }
        try {
            setupGattServerAndAdvertising()
        } catch (e: Exception) {
            Log.e(TAG, "BLE GATT / advertising failed: ${e.message}", e)
            bleRepository.setGattRunning(false)
            try {
                startForeground(
                    1001,
                    createFailureNotification("Bluetooth failed to start. Check that Bluetooth and permissions are enabled.")
                )
            } catch (_: Exception) { }
            try { stopSelf() } catch (_: Exception) { }
        }
    }

    @SuppressLint("MissingPermission")
    private fun setupGattServerAndAdvertising() {
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val adapter = bluetoothManager.adapter ?: run {
            Log.w(TAG, "Bluetooth adapter is null")
            bleRepository.setGattRunning(false)
            bleRepository.setBluetoothEnabled(false)
            sendBroadcast(Intent(ACTION_BLE_UNAVAILABLE))
            return
        }
        if (!adapter.isEnabled) {
            Log.w(TAG, "Bluetooth is disabled")
            bleRepository.setGattRunning(false)
            bleRepository.setBluetoothEnabled(false)
            sendBroadcast(Intent(ACTION_BLE_UNAVAILABLE))
            return
        }

        // Bluetooth is on — reset the flag in case it was previously disabled
        bleRepository.setBluetoothEnabled(true)

        gattServer = try {
            bluetoothManager.openGattServer(this, gattCallback)
        } catch (e: Exception) {
            Log.e(TAG, "openGattServer: ${e.message}", e)
            null
        }
        if (gattServer == null) {
            bleRepository.setGattRunning(false)
            return
        }
        val service = BluetoothGattService(
            UUID.fromString(BleConstants.SERVICE_UUID),
            BluetoothGattService.SERVICE_TYPE_PRIMARY
        )
        val writeCharacteristic = BluetoothGattCharacteristic(
            UUID.fromString(BleConstants.SYNC_CHAR_UUID),
            BluetoothGattCharacteristic.PROPERTY_WRITE,
            BluetoothGattCharacteristic.PERMISSION_WRITE
        )
        service.addCharacteristic(writeCharacteristic)
        gattServer?.addService(service)

        advertiser = adapter.bluetoothLeAdvertiser
        val settings = AdvertiseSettings.Builder()
            .setConnectable(true)
            .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
            .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM)
            .build()
        val data = AdvertiseData.Builder()
            .addServiceUuid(android.os.ParcelUuid(UUID.fromString(BleConstants.SERVICE_UUID)))
            .setIncludeDeviceName(false)
            .build()
        try {
            advertiser?.startAdvertising(settings, data, object : AdvertiseCallback() {})
        } catch (e: Exception) {
            Log.e(TAG, "startAdvertising: ${e.message}", e)
            bleRepository.setGattRunning(false)
            return
        }
        bleRepository.setGattRunning(true)
    }

    private fun createNotificationChannel() {
        val manager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        val channel = NotificationChannel(
            BleConstants.NOTIF_CHANNEL_ID,
            "AcouSense BLE",
            NotificationManager.IMPORTANCE_LOW
        )
        manager.createNotificationChannel(channel)
    }

    private fun createNotification(): Notification {
        return NotificationCompat.Builder(this, BleConstants.NOTIF_CHANNEL_ID)
            .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
            .setContentTitle("AcouSense BLE active")
            .setContentText("Listening for sync packets from nearby device")
            .setOngoing(true)
            .build()
    }

    private fun createFailureNotification(message: String): Notification {
        return NotificationCompat.Builder(this, BleConstants.NOTIF_CHANNEL_ID)
            .setSmallIcon(android.R.drawable.stat_notify_error)
            .setContentTitle("AcouSense BLE")
            .setContentText(message)
            .build()
    }

    @SuppressLint("MissingPermission")
    override fun onDestroy() {
        bleRepository.setGattRunning(false)
        runCatching { advertiser?.stopAdvertising(object : AdvertiseCallback() {}) }
        runCatching { gattServer?.close() }
        serviceScope.cancel()
        super.onDestroy()
    }
}
