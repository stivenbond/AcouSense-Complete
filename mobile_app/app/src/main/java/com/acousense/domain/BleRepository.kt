package com.acousense.domain

import com.acousense.data.ble.SyncPayload
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class BleRepository @Inject constructor() {
    private val _events = MutableSharedFlow<SyncPayload>(extraBufferCapacity = 32)
    val events: SharedFlow<SyncPayload> = _events

    private val _gattRunning = MutableStateFlow(false)
    val gattRunning: StateFlow<Boolean> = _gattRunning.asStateFlow()

    /** True when the Bluetooth adapter is enabled, false when BT is off. */
    private val _bluetoothEnabled = MutableStateFlow(true)
    val bluetoothEnabled: StateFlow<Boolean> = _bluetoothEnabled.asStateFlow()

    fun setGattRunning(running: Boolean) {
        _gattRunning.value = running
    }

    fun setBluetoothEnabled(enabled: Boolean) {
        _bluetoothEnabled.value = enabled
    }

    suspend fun emit(payload: SyncPayload) {
        _events.emit(payload)
    }
}
