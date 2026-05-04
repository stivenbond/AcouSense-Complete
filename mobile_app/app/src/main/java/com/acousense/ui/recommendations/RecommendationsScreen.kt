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
import com.acousense.ui.aiadvice.AiAdviceScreen

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
            AiAdviceScreen(
                onGoMonitor = {},
                onGoSettings = onOpenSettings,
            )
        }
    }
}
