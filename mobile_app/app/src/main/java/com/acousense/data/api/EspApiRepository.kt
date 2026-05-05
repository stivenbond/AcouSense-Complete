package com.acousense.data.api

import com.acousense.data.settings.SettingsRepository
import javax.inject.Inject
import javax.inject.Singleton
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL

data class EspStatusSnapshot(
    val isReachable: Boolean = false,
    val arduinoOnline: Boolean = false,
    val sdCardOk: Boolean = false,
    val bluetoothStatus: String = "unknown",
    val wifiRssi: Int? = null,
    val lastReadingAvg: Int? = null,
    val lastReadingMax: Int? = null,
    val lastReadingAlertLevel: Int? = null,
    val readingCount: Int = 0,
    val errorMessage: String? = null,
)

@Singleton
class EspApiRepository @Inject constructor(
    private val settingsRepository: SettingsRepository,
) {
    private val _status = MutableStateFlow(EspStatusSnapshot())
    val status: StateFlow<EspStatusSnapshot> = _status.asStateFlow()

    suspend fun refresh(): EspStatusSnapshot = withContext(Dispatchers.IO) {
        runCatching {
            val statusJson = getJsonObject("/api/status")
            val readingsJson = getJsonObject("/api/readings?limit=10")
            val data = readingsJson.optJSONArray("data") ?: JSONArray()
            val firstReading = if (data.length() > 0) data.optJSONObject(data.length() - 1) else null

            EspStatusSnapshot(
                isReachable = true,
                arduinoOnline = statusJson.optString("arduino") == "online",
                sdCardOk = statusJson.optString("sd_card") == "ok",
                bluetoothStatus = statusJson.optString("bt_status", "unknown"),
                wifiRssi = statusJson.optInt("wifi_rssi"),
                lastReadingAvg = firstReading?.optInt("avg_level"),
                lastReadingMax = firstReading?.optInt("max_level"),
                lastReadingAlertLevel = firstReading?.optInt("alert_level"),
                readingCount = readingsJson.optInt("total", data.length()),
                errorMessage = null,
            )
        }.getOrElse { error ->
            EspStatusSnapshot(
                isReachable = false,
                errorMessage = error.message ?: "Unable to reach ESP API",
            )
        }.also { _status.value = it }
    }

    private fun getJsonObject(path: String): JSONObject {
        val base = settingsRepository.getEspApiBaseUrl().trimEnd('/')
        val url = URL("$base$path")
        val connection = (url.openConnection() as HttpURLConnection).apply {
            requestMethod = "GET"
            connectTimeout = 5000
            readTimeout = 5000
            setRequestProperty("Accept", "application/json")
        }

        return connection.useJsonObject()
    }
}

private fun HttpURLConnection.useJsonObject(): JSONObject {
    try {
        val stream = if (responseCode in 200..299) inputStream else errorStream
        val payload = stream?.bufferedReader()?.use { it.readText() }.orEmpty()
        if (responseCode !in 200..299) {
            throw IllegalStateException("HTTP $responseCode: $payload")
        }
        return JSONObject(payload)
    } finally {
        disconnect()
    }
}
