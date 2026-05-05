package com.acousense.data.ble

import java.nio.ByteBuffer
import java.nio.ByteOrder

data class SyncPayload(
    val deviceIdentifier: String,
    val userIdentifier: String,
    val periodStart: Long,
    val periodEnd: Long,
    val avgLevel: Int,
    val maxLevel: Int,
    val exposureSeconds: Long,
    val classification: Int
)

object SyncPayloadParser {
    private const val PAYLOAD_SIZE = 52

    // Matches shared/ble_constants.h and the ESP32 firmware.
    fun parse(bytes: ByteArray): Result<SyncPayload> = runCatching {
        require(bytes.size == PAYLOAD_SIZE) { "Invalid payload length: ${bytes.size}" }

        val buffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
        val deviceBytes = ByteArray(6)
        buffer.get(deviceBytes)
        val userBytes = ByteArray(32)
        buffer.get(userBytes)
        val periodStart = buffer.int.toLong() and 0xFFFFFFFFL
        val periodEnd = buffer.int.toLong() and 0xFFFFFFFFL
        val userIdentifier = userBytes.toString(Charsets.UTF_8).trim('\u0000', ' ')
        val deviceIdentifier = deviceBytes.joinToString(":") { "%02X".format(it.toInt() and 0xFF) }

        val avgLevel = buffer.short.toInt() and 0xFFFF
        val maxLevel = buffer.short.toInt() and 0xFFFF
        val classification = buffer.get().toInt() and 0xFF
        buffer.get() // reserved
        val exposureSeconds = (periodEnd - periodStart).coerceAtLeast(0L)

        SyncPayload(
            deviceIdentifier = deviceIdentifier,
            userIdentifier = userIdentifier,
            periodStart = periodStart,
            periodEnd = periodEnd,
            avgLevel = avgLevel,
            maxLevel = maxLevel,
            exposureSeconds = exposureSeconds,
            classification = classification
        )
    }
}
