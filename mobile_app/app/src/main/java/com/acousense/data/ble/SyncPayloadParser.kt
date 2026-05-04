package com.acousense.data.ble

import java.nio.ByteBuffer
import java.nio.ByteOrder

data class SyncPayload(
    val version: Int,
    val userIdentifier: String,
    val periodStart: Long,
    val periodEnd: Long,
    val avgLevel: Int,
    val maxLevel: Int,
    val peakCount: Int,
    val exposureSeconds: Long,
    val classification: Int
)

object SyncPayloadParser {
    private const val PAYLOAD_SIZE = 52

    /**
     * Binary layout (little-endian, 52 bytes total):
     * | offset | size | field              |
     * |--------|------|--------------------|
     * | 0      | 1    | version            |
     * | 1      | 1    | device_count       |
     * | 2      | 4    | periodStart        |
     * | 6      | 4    | periodEnd          |
     * | 10     | 16   | userIdentifier     |
     * | 26     | 2    | avgLevel           |
     * | 28     | 2    | maxLevel           |
     * | 30     | 2    | peakCount          |
     * | 32     | 4    | exposureSeconds    |
     * | 36     | 1    | classification     |
     * | 37     | 15   | reserved / padding |
     */
    fun parse(bytes: ByteArray): Result<SyncPayload> = runCatching {
        require(bytes.size == PAYLOAD_SIZE) { "Invalid payload length: ${bytes.size}" }

        val buffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
        val version = buffer.get().toInt() and 0xFF
        buffer.get() // device_count (unused)
        val periodStart = buffer.int.toLong() and 0xFFFFFFFFL
        val periodEnd = buffer.int.toLong() and 0xFFFFFFFFL

        val userBytes = ByteArray(16)
        buffer.get(userBytes)
        val userIdentifier = userBytes.toString(Charsets.UTF_8).trim('\u0000', ' ')

        val avgLevel = buffer.short.toInt() and 0xFFFF
        val maxLevel = buffer.short.toInt() and 0xFFFF
        val peakCount = buffer.short.toInt() and 0xFFFF
        val exposureSeconds = buffer.int.toLong() and 0xFFFFFFFFL
        val classification = buffer.get().toInt() and 0xFF
        buffer.position(buffer.position() + 15)

        SyncPayload(
            version = version,
            userIdentifier = userIdentifier,
            periodStart = periodStart,
            periodEnd = periodEnd,
            avgLevel = avgLevel,
            maxLevel = maxLevel,
            peakCount = peakCount,
            exposureSeconds = exposureSeconds,
            classification = classification
        )
    }
}
