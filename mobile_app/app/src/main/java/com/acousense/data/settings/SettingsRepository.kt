package com.acousense.data.settings

import android.content.SharedPreferences
import javax.inject.Inject
import javax.inject.Singleton
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

data class InferenceSettings(
    val maxTokens: Int = 1024,
    val temperature: Float = 0.15f,
    val topK: Int = 8,
)

@Singleton
class SettingsRepository @Inject constructor(
    private val prefs: SharedPreferences,
) {
    companion object {
        const val DEFAULT_MODEL_PATH = "/data/local/tmp/acousense/gemma.bin"
        private const val KEY_MODEL_PATH = "model_path"
        private const val KEY_MAX_TOKENS = "max_tokens"
        private const val KEY_TEMPERATURE = "temperature"
        private const val KEY_TOP_K = "top_k"
        private const val KEY_ESP_API_BASE_URL = "esp_api_base_url"
    }

    private val _modelPath = MutableStateFlow(getModelPath())
    val modelPath: StateFlow<String> = _modelPath.asStateFlow()

    private val _inferenceSettings = MutableStateFlow(loadInferenceSettings())
    val inferenceSettings: StateFlow<InferenceSettings> = _inferenceSettings.asStateFlow()

    private val _espApiBaseUrl = MutableStateFlow(getEspApiBaseUrl())
    val espApiBaseUrl: StateFlow<String> = _espApiBaseUrl.asStateFlow()

    fun getModelPath(): String = prefs.getString(KEY_MODEL_PATH, DEFAULT_MODEL_PATH) ?: DEFAULT_MODEL_PATH

    fun setModelPath(path: String) {
        prefs.edit().putString(KEY_MODEL_PATH, path).apply()
        _modelPath.value = path
    }

    fun getEspApiBaseUrl(): String = prefs.getString(KEY_ESP_API_BASE_URL, "http://192.168.4.1") ?: "http://192.168.4.1"

    fun setEspApiBaseUrl(url: String) {
        val normalized = url.trim().trimEnd('/')
        prefs.edit().putString(KEY_ESP_API_BASE_URL, normalized).apply()
        _espApiBaseUrl.value = normalized
    }

    fun updateInferenceSettings(
        maxTokens: Int = _inferenceSettings.value.maxTokens,
        temperature: Float = _inferenceSettings.value.temperature,
        topK: Int = _inferenceSettings.value.topK,
    ) {
        prefs.edit()
            .putInt(KEY_MAX_TOKENS, maxTokens)
            .putFloat(KEY_TEMPERATURE, temperature)
            .putInt(KEY_TOP_K, topK)
            .apply()
        _inferenceSettings.value = InferenceSettings(
            maxTokens = maxTokens,
            temperature = temperature,
            topK = topK,
        )
    }

    private fun loadInferenceSettings(): InferenceSettings = InferenceSettings(
        maxTokens = prefs.getInt(KEY_MAX_TOKENS, 1024),
        temperature = prefs.getFloat(KEY_TEMPERATURE, 0.15f),
        topK = prefs.getInt(KEY_TOP_K, 8),
    )
}
