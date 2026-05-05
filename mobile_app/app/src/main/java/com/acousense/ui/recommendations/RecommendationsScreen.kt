package com.acousense.ui.recommendations

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.SmartToy
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SuggestionChip
import androidx.compose.material3.SuggestionChipDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.acousense.ui.HealthEmptyState
import com.acousense.ui.SurfaceCard
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun RecommendationsScreen(
    selectedDate: String,
    onOpenSettings: () -> Unit = {},
    viewModel: RecommendationsViewModel = hiltViewModel(),
) {
    LaunchedEffect(selectedDate) { viewModel.load(selectedDate) }
    val state by viewModel.state.collectAsStateWithLifecycle()

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("AI Advice") },
                actions = {
                    if (!state.isModelAvailable) {
                        SuggestionChip(
                            onClick = onOpenSettings,
                            label = { Text("Demo mode", style = MaterialTheme.typography.labelSmall) },
                            icon = { Icon(Icons.Outlined.SmartToy, contentDescription = null, modifier = Modifier.padding(end = 2.dp)) },
                            colors = SuggestionChipDefaults.suggestionChipColors(
                                containerColor = MaterialTheme.colorScheme.surfaceContainerHigh,
                                labelColor = MaterialTheme.colorScheme.onSurfaceVariant,
                                iconContentColor = MaterialTheme.colorScheme.onSurfaceVariant,
                            ),
                            border = SuggestionChipDefaults.suggestionChipBorder(
                                enabled = true,
                                borderColor = MaterialTheme.colorScheme.outlineVariant,
                                borderWidth = 0.5.dp,
                            ),
                        )
                    }
                },
            )
        },
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding),
            verticalArrangement = Arrangement.Top,
        ) {
            val summary = state.dailySummary
            if (summary == null) {
                HealthEmptyState(
                    icon = Icons.Outlined.SmartToy,
                    title = "No summary selected",
                    subtitle = "Open a day from History to generate AI guidance.",
                    actionLabel = "Settings",
                    onAction = onOpenSettings,
                )
            } else {
                SurfaceCard(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(16.dp),
                ) {
                    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                        Text(
                            text = "Advice for ${summary.date}",
                            style = MaterialTheme.typography.titleMedium,
                            color = MaterialTheme.colorScheme.onSurface,
                        )
                        if (state.streamedText.isNotBlank()) {
                            Text(
                                text = state.streamedText,
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        } else if (!summary.aiRecommendation.isNullOrBlank()) {
                            Text(
                                text = summary.aiRecommendation,
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        } else {
                            Text(
                                text = if (state.isModelAvailable) {
                                    "No advice generated yet."
                                } else {
                                    "Model not available. You can still use BLE sync and add the local model later."
                                },
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                        Spacer(Modifier.height(4.dp))
                        Button(
                            onClick = { viewModel.generateAdvice() },
                            enabled = !state.isGenerating,
                        ) {
                            if (state.isGenerating) {
                                CircularProgressIndicator(
                                    modifier = Modifier.height(18.dp),
                                    strokeWidth = 2.dp,
                                    color = MaterialTheme.colorScheme.onPrimary,
                                )
                            } else {
                                Text(if (state.isModelAvailable) "Generate Advice" else "Retry After Model Setup")
                            }
                        }
                    }
                }
            }
        }
    }
}
