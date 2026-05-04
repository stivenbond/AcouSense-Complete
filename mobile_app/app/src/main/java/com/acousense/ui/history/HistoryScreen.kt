package com.acousense.ui.history

import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.ArrowForward
import androidx.compose.material.icons.outlined.BarChart
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.acousense.data.db.DailySummary
import com.acousense.ui.HealthEmptyState
import com.acousense.ui.NoiseClassificationBadge
import com.acousense.ui.StatCard
import com.acousense.ui.formatExposure
import com.acousense.ui.formatLevelDb
import com.acousense.ui.noiseColor
import java.time.LocalDate
import java.time.format.DateTimeFormatter
import kotlin.math.roundToInt

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HistoryScreen(
    onViewAdvice: (String) -> Unit = {},
    viewModel: HistoryViewModel = hiltViewModel(),
) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val summaries = state.summaries

    val avgPeak by remember(summaries) {
        derivedStateOf {
            if (summaries.isEmpty()) "—"
            else formatLevelDb(summaries.map { it.maxLevel }.average().roundToInt())
        }
    }
    val totalExposure by remember(summaries) {
        derivedStateOf {
            if (summaries.isEmpty()) "—"
            else formatExposure(summaries.sumOf { it.totalExposureSeconds })
        }
    }

    Scaffold(
        containerColor = MaterialTheme.colorScheme.background,
        contentWindowInsets = WindowInsets(0, 0, 0, 0),
        topBar = {
            TopAppBar(
                title = { Text("Exposure History") },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.background,
                    titleContentColor = MaterialTheme.colorScheme.onBackground,
                ),
            )
        },
    ) { innerPadding ->
        if (summaries.isEmpty()) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(innerPadding),
            ) {
                HealthEmptyState(
                    icon = Icons.Outlined.BarChart,
                    title = "No exposure data yet",
                    subtitle = "Connect your AcouSense sensor to start recording",
                )
            }
        } else {
            LazyColumn(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(innerPadding),
                contentPadding = PaddingValues(horizontal = 16.dp, vertical = 16.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                item {
                    SingleChoiceSegmentedButtonRow(modifier = Modifier.fillMaxWidth()) {
                        listOf("7D", "30D", "ALL").forEachIndexed { index, label ->
                            SegmentedButton(
                                selected = state.range == label,
                                onClick = { viewModel.setRange(label) },
                                shape = SegmentedButtonDefaults.itemShape(index = index, count = 3),
                            ) {
                                Text(label, style = MaterialTheme.typography.labelMedium)
                            }
                        }
                    }
                }

                item {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        StatCard(
                            label = "DAYS TRACKED",
                            value = summaries.size.toString(),
                            modifier = Modifier
                                .weight(1f)
                                .height(80.dp),
                        )
                        StatCard(
                            label = "AVG PEAK",
                            value = avgPeak,
                            modifier = Modifier
                                .weight(1f)
                                .height(80.dp),
                        )
                        StatCard(
                            label = "TOTAL EXPOSURE",
                            value = totalExposure,
                            modifier = Modifier
                                .weight(1f)
                                .height(80.dp),
                        )
                    }
                }

                item {
                    ExposureBarChart(
                        summaries = summaries.sortedBy { it.date },
                        modifier = Modifier.fillMaxWidth(),
                    )
                }

                items(
                    items = summaries,
                    key = { it.date },
                ) { summary ->
                    DailySummaryCard(
                        summary = summary,
                        onViewAdvice = { onViewAdvice(summary.date) },
                    )
                }
            }
        }
    }
}

@Composable
private fun ExposureBarChart(
    summaries: List<DailySummary>,
    modifier: Modifier = Modifier,
) {
    var playAnimation by remember(summaries) { mutableStateOf(false) }
    val labelStyle = MaterialTheme.typography.labelSmall
    val onSurfaceVariant = MaterialTheme.colorScheme.onSurfaceVariant
    val chartScale by animateFloatAsState(
        targetValue = if (playAnimation) 1f else 0f,
        animationSpec = spring(stiffness = Spring.StiffnessLow),
        label = "history_bar_scale",
    )
    val textMeasurer = rememberTextMeasurer()

    LaunchedEffect(summaries) {
        playAnimation = true
    }

    Card(
        modifier = modifier.height(200.dp),
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceContainerLow,
        ),
    ) {
        Canvas(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 12.dp, vertical = 16.dp),
        ) {
            if (summaries.isEmpty()) return@Canvas

            val labelHeight = 24.dp.toPx()
            val chartHeight = size.height - labelHeight
            val spacing = 8.dp.toPx()
            val count = summaries.size
            val barWidth = ((size.width - spacing * (count + 1)) / count).coerceAtLeast(10.dp.toPx())

            summaries.forEachIndexed { index, summary ->
                val x = spacing + index * (barWidth + spacing)
                val fraction = (summary.maxLevel / 1023f).coerceIn(0f, 1f) * chartScale
                val barHeight = fraction * (chartHeight - 6.dp.toPx())
                val y = chartHeight - barHeight
                val barColor = noiseColor(summary.dominantClassification)
                val dayLabel = runCatching {
                    LocalDate.parse(summary.date).format(DateTimeFormatter.ofPattern("EEE"))
                }.getOrDefault("—")

                drawRoundRect(
                    color = barColor,
                    topLeft = Offset(x, y),
                    size = Size(barWidth, barHeight),
                    cornerRadius = CornerRadius(4.dp.toPx(), 4.dp.toPx()),
                )
                drawText(
                    textMeasurer = textMeasurer,
                    text = dayLabel,
                    topLeft = Offset(x, size.height - labelHeight),
                    style = TextStyle(
                        color = onSurfaceVariant,
                        fontSize = labelStyle.fontSize,
                        fontWeight = labelStyle.fontWeight,
                    ),
                )
            }
        }
    }
}

@Composable
private fun DailySummaryCard(
    summary: DailySummary,
    onViewAdvice: () -> Unit,
) {
    val formattedDate = remember(summary.date) {
        runCatching {
            LocalDate.parse(summary.date).format(DateTimeFormatter.ofPattern("EEE dd MMM"))
        }.getOrDefault(summary.date)
    }

    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceContainerLow,
        ),
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            androidx.compose.foundation.layout.Column(
                modifier = Modifier.weight(1f),
                verticalArrangement = Arrangement.spacedBy(6.dp),
            ) {
                Text(
                    text = formattedDate,
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold,
                    color = MaterialTheme.colorScheme.onSurface,
                )
                Text(
                    text = "${formatExposure(summary.totalExposureSeconds)} · Peak: ${formatLevelDb(summary.maxLevel)} · ${summary.sessionCount} sessions",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            androidx.compose.foundation.layout.Column(
                horizontalAlignment = Alignment.End,
                verticalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                NoiseClassificationBadge(level = summary.dominantClassification)
                TextButton(onClick = onViewAdvice) {
                    Text("AI Advice")
                    Icon(
                        imageVector = Icons.AutoMirrored.Outlined.ArrowForward,
                        contentDescription = null,
                        modifier = Modifier.padding(start = 4.dp),
                    )
                }
            }
        }
    }
}
