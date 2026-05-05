package com.acousense.ui.settings

import android.content.Context
import android.content.SharedPreferences
import android.net.Uri
import android.provider.OpenableColumns
import androidx.compose.runtime.Immutable
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.acousense.data.ai.GemmaInference
import com.acousense.data.settings.SettingsRepository
import com.acousense.domain.ExposureRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import java.io.File
import javax.inject.Inject
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@Immutable
data class SettingsUiState(
    val modelPath: String = SettingsRepository.DEFAULT_MODEL_PATH,
    val espApiBaseUrl: String = "http://192.168.4.1",
    val retentionDays: Int = 42,
    val modelExists: Boolean = false,
    val maxTokens: Int = 1024,
    val snackbarMessage: String? = null,
)

@HiltViewModel
class SettingsViewModel @Inject constructor(
    private val prefs: SharedPreferences,
    private val repository: ExposureRepository,
    private val settingsRepository: SettingsRepository,
    private val gemmaInference: GemmaInference,
) : ViewModel() {
    private val _state = MutableStateFlow(
        SettingsUiState(
            modelPath = settingsRepository.getModelPath(),
            espApiBaseUrl = settingsRepository.getEspApiBaseUrl(),
            retentionDays = prefs.getInt("retention_days", 42),
            modelExists = File(settingsRepository.getModelPath()).exists(),
            maxTokens = settingsRepository.inferenceSettings.value.maxTokens,
        ),
    )
    val state: StateFlow<SettingsUiState> = _state.asStateFlow()

    init {
        viewModelScope.launch {
            settingsRepository.modelPath.collect { modelPath ->
                _state.update {
                    it.copy(
                        modelPath = modelPath,
                        modelExists = File(modelPath).exists(),
                    )
                }
            }
        }

        viewModelScope.launch {
            settingsRepository.espApiBaseUrl.collect { baseUrl ->
                _state.update { it.copy(espApiBaseUrl = baseUrl) }
            }
        }

        viewModelScope.launch {
            settingsRepository.inferenceSettings.collect { settings ->
                _state.update { it.copy(maxTokens = settings.maxTokens) }
            }
        }
    }

    fun consumeSnackbar() {
        _state.update { it.copy(snackbarMessage = null) }
    }

    fun retryModelLoad() {
        val modelPath = settingsRepository.getModelPath()
        val modelExists = File(modelPath).exists()
        _state.update { it.copy(modelExists = modelExists) }

        if (!modelExists) {
            _state.update { it.copy(snackbarMessage = "Model file not found at the configured path.") }
            return
        }

        viewModelScope.launch(Dispatchers.IO) {
            val result = gemmaInference.initialize()
            _state.update {
                it.copy(
                    modelExists = result.isSuccess,
                    snackbarMessage = result.exceptionOrNull()?.localizedMessage ?: "Model is ready.",
                )
            }
        }
    }

    fun updateMaxTokens(value: Int) {
        settingsRepository.updateInferenceSettings(maxTokens = value.coerceIn(256, 2048))
        gemmaInference.close()
    }

    fun updateRetentionDays(value: Int) {
        val boundedValue = value.coerceIn(7, 90)
        prefs.edit().putInt("retention_days", boundedValue).apply()
        _state.update { it.copy(retentionDays = boundedValue) }
    }

    fun updateEspApiBaseUrl(value: String) {
        settingsRepository.setEspApiBaseUrl(value)
        _state.update { it.copy(snackbarMessage = "ESP API base URL updated.") }
    }

    fun clearAllData() {
        viewModelScope.launch(Dispatchers.IO) {
            repository.clearAllData()
            withContext(Dispatchers.Main) {
                _state.update { it.copy(snackbarMessage = "All exposure data has been cleared.") }
            }
        }
    }

    fun importModelFile(context: Context, uri: Uri) {
        viewModelScope.launch(Dispatchers.IO) {
            _state.update { it.copy(snackbarMessage = "Importing model file...") }

            runCatching {
                val modelsDirectory = File(context.filesDir, "models").apply { mkdirs() }
                val fileName = readDisplayName(context, uri)
                    ?.takeIf { it.isNotBlank() }
                    ?: "gemma-4-E2B-it.litertlm"
                val safeName = if (fileName.endsWith(".litertlm", ignoreCase = true)) {
                    fileName
                } else {
                    "$fileName.litertlm"
                }
                val targetFile = File(modelsDirectory, safeName)

                context.contentResolver.openInputStream(uri).use { inputStream ->
                    requireNotNull(inputStream) { "Unable to open the selected model file." }
                    targetFile.outputStream().use { outputStream ->
                        inputStream.copyTo(outputStream)
                    }
                }

                if (!targetFile.exists() || targetFile.length() <= 0L) {
                    error("The imported model file appears to be empty.")
                }

                withContext(Dispatchers.Main) {
                    settingsRepository.setModelPath(targetFile.absolutePath)
                    gemmaInference.close()
                    _state.update {
                        it.copy(
                            modelPath = targetFile.absolutePath,
                            modelExists = true,
                            snackbarMessage = "Model imported successfully.",
                        )
                    }
                }
            }.onFailure { error ->
                withContext(Dispatchers.Main) {
                    _state.update {
                        it.copy(
                            snackbarMessage = error.localizedMessage ?: "Unable to import the model file.",
                        )
                    }
                }
            }
        }
    }

    private fun readDisplayName(context: Context, uri: Uri): String? {
        val projection = arrayOf(OpenableColumns.DISPLAY_NAME)
        context.contentResolver.query(uri, projection, null, null, null)?.use { cursor ->
            val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (index >= 0 && cursor.moveToFirst()) {
                return cursor.getString(index)
            }
        }
        return null
    }
}
