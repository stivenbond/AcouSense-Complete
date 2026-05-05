package com.acousense.ui

import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.acousense.ui.theme.NoiseHigh
import com.acousense.ui.theme.NoiseLow
import com.acousense.ui.theme.NoiseModerate
import com.acousense.ui.theme.NoiseNone
import kotlin.math.cos
import kotlin.math.sin

@Composable
fun ExposureArcGauge(
    value: Int,
    classification: Int,
    modifier: Modifier = Modifier,
) {
    val levelDb = rawLevelToDecibels(value)
    val animatedFraction by animateFloatAsState(
        targetValue = value.coerceIn(0, 1023) / 1023f,
        animationSpec = spring(
            dampingRatio = Spring.DampingRatioMediumBouncy,
            stiffness = Spring.StiffnessLow,
        ),
        label = "gauge_fraction",
    )
    val arcColor = noiseColor(classification)
    val trackColor = MaterialTheme.colorScheme.surfaceContainerHighest
    val glowColor = arcColor.copy(alpha = 0.25f)
    val tipHighlight = MaterialTheme.colorScheme.onBackground.copy(alpha = 0.9f)

    Box(modifier = modifier, contentAlignment = Alignment.Center) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            val sweepAngle = animatedFraction * 240f
            val strokeWidth = 22.dp.toPx()
            val inset = strokeWidth / 2f
            val arcRect = Rect(
                left = inset,
                top = inset,
                right = size.width - inset,
                bottom = size.height - inset,
            )
            val center = Offset(size.width / 2f, size.height / 2f)

            drawArc(
                color = glowColor,
                startAngle = 150f,
                sweepAngle = sweepAngle,
                useCenter = false,
                topLeft = Offset(arcRect.left - 4.dp.toPx(), arcRect.top - 4.dp.toPx()),
                size = Size(arcRect.width + 8.dp.toPx(), arcRect.height + 8.dp.toPx()),
                style = Stroke(width = strokeWidth + 8.dp.toPx(), cap = StrokeCap.Round),
            )

            drawArc(
                color = trackColor,
                startAngle = 150f,
                sweepAngle = 240f,
                useCenter = false,
                topLeft = Offset(arcRect.left, arcRect.top),
                size = Size(arcRect.width, arcRect.height),
                style = Stroke(width = strokeWidth, cap = StrokeCap.Round),
            )

            if (sweepAngle > 0f) {
                drawArc(
                    brush = Brush.sweepGradient(
                        colors = listOf(arcColor.copy(alpha = 0.6f), arcColor),
                        center = center,
                    ),
                    startAngle = 150f,
                    sweepAngle = sweepAngle,
                    useCenter = false,
                    topLeft = Offset(arcRect.left, arcRect.top),
                    size = Size(arcRect.width, arcRect.height),
                    style = Stroke(width = strokeWidth, cap = StrokeCap.Round),
                )

                val angleRad = Math.toRadians((150f + sweepAngle).toDouble())
                val radius = arcRect.width / 2f
                val cx = center.x + radius * cos(angleRad).toFloat()
                val cy = center.y + radius * sin(angleRad).toFloat()
                val tipCenter = Offset(cx, cy)

                drawCircle(color = arcColor, radius = strokeWidth / 2f, center = tipCenter)
                drawCircle(color = tipHighlight, radius = 4.dp.toPx(), center = tipCenter)
            }
        }

        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(
                text = "${levelDb} dB",
                style = MaterialTheme.typography.displayMedium,
                color = MaterialTheme.colorScheme.onBackground,
            )
            Text(
                text = "SPL",
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Spacer(Modifier.height(4.dp))
            NoiseClassificationBadge(level = classification)
        }
    }
}

@Composable
fun NoiseClassificationBadge(level: Int, modifier: Modifier = Modifier) {
    val color = noiseColor(level)
    val label = noiseLabel(level)

    Surface(
        modifier = modifier,
        shape = MaterialTheme.shapes.extraLarge,
        color = color.copy(alpha = 0.12f),
        border = BorderStroke(1.dp, color.copy(alpha = 0.4f)),
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 10.dp, vertical = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(5.dp),
        ) {
            Box(
                modifier = Modifier
                    .size(6.dp)
                    .background(color, CircleShape),
            )
            Text(
                text = label,
                style = MaterialTheme.typography.labelMedium,
                color = color,
            )
        }
    }
}

@Composable
fun StatCard(
    label: String,
    value: String,
    dotColor: Color = MaterialTheme.colorScheme.primary,
    modifier: Modifier = Modifier,
    isLoading: Boolean = false,
) {
    Card(
        modifier = modifier,
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceContainerLow,
        ),
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(5.dp),
            ) {
                Box(
                    modifier = Modifier
                        .size(5.dp)
                        .background(dotColor, CircleShape),
                )
                Text(
                    text = label,
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }

            if (isLoading) {
                ShimmerBlock(
                    modifier = Modifier
                        .fillMaxWidth(0.6f)
                        .height(28.dp),
                )
            } else {
                Text(
                    text = value,
                    style = MaterialTheme.typography.titleLarge,
                    color = MaterialTheme.colorScheme.onSurface,
                )
            }
        }
    }
}

@Composable
fun PulsingDot(color: Color, size: Dp = 8.dp) {
    val alpha by rememberInfiniteTransition(label = "pulse").animateFloat(
        initialValue = 0.3f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(900, easing = FastOutSlowInEasing),
            repeatMode = RepeatMode.Reverse,
        ),
        label = "pulse_alpha",
    )

    Box(
        modifier = Modifier
            .size(size)
            .background(color.copy(alpha = alpha), CircleShape),
    )
}

@Composable
fun SectionHeader(title: String) {
    Text(
        text = title.uppercase(),
        style = MaterialTheme.typography.labelSmall,
        color = MaterialTheme.colorScheme.primary,
        letterSpacing = 1.5.sp,
        modifier = Modifier.padding(top = 8.dp, bottom = 4.dp),
    )
}

@Composable
fun HealthEmptyState(
    icon: ImageVector,
    title: String,
    subtitle: String,
    actionLabel: String? = null,
    onAction: (() -> Unit)? = null,
) {
    Column(
        modifier = Modifier.fillMaxSize(),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        Surface(
            shape = CircleShape,
            color = MaterialTheme.colorScheme.surfaceContainerHigh,
            modifier = Modifier.size(96.dp),
        ) {
            Icon(
                imageVector = icon,
                contentDescription = null,
                modifier = Modifier.padding(24.dp),
                tint = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.5f),
            )
        }
        Spacer(Modifier.height(20.dp))
        Text(
            text = title,
            style = MaterialTheme.typography.headlineSmall,
            textAlign = TextAlign.Center,
            color = MaterialTheme.colorScheme.onSurface,
        )
        Spacer(Modifier.height(6.dp))
        Text(
            text = subtitle,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            textAlign = TextAlign.Center,
            modifier = Modifier.padding(horizontal = 32.dp),
        )
        if (actionLabel != null && onAction != null) {
            Spacer(Modifier.height(24.dp))
            FilledTonalButton(onClick = onAction) {
                Text(actionLabel)
            }
        }
    }
}

@Composable
fun SurfaceCard(
    modifier: Modifier = Modifier,
    contentPadding: PaddingValues = PaddingValues(16.dp),
    content: @Composable () -> Unit,
) {
    Card(
        modifier = modifier,
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceContainerLow,
        ),
    ) {
        Box(modifier = Modifier.padding(contentPadding)) {
            content()
        }
    }
}

@Composable
private fun ShimmerBlock(modifier: Modifier = Modifier) {
    val alpha by rememberInfiniteTransition(label = "stat_shimmer").animateFloat(
        initialValue = 0.45f,
        targetValue = 0.9f,
        animationSpec = infiniteRepeatable(
            animation = tween(900, easing = FastOutSlowInEasing),
            repeatMode = RepeatMode.Reverse,
        ),
        label = "stat_shimmer_alpha",
    )

    Box(
        modifier = modifier.background(
            color = MaterialTheme.colorScheme.surfaceContainerHighest.copy(alpha = alpha),
            shape = MaterialTheme.shapes.extraLarge,
        ),
    )
}

fun noiseColor(level: Int): Color = when (level) {
    1 -> NoiseLow
    2 -> NoiseModerate
    3, 4 -> NoiseHigh
    else -> NoiseNone
}

fun noiseLabel(level: Int): String = when (level) {
    1 -> "CAUTION"
    2 -> "MODERATE"
    3 -> "HIGH"
    4 -> "DANGER"
    else -> "SAFE"
}

fun formatExposure(totalExposureSeconds: Long): String {
    if (totalExposureSeconds <= 0L) return "0m"
    val hours = totalExposureSeconds / 3600
    val minutes = (totalExposureSeconds % 3600) / 60
    return when {
        hours > 0 && minutes > 0 -> "${hours}h ${minutes}m"
        hours > 0 -> "${hours}h"
        minutes > 0 -> "${minutes}m"
        else -> "<1m"
    }
}

fun rawLevelToDecibels(rawLevel: Int): Int {
    val clamped = rawLevel.coerceIn(0, 1023)
    return (30f + (clamped / 1023f) * 90f).toInt()
}

fun formatLevelDb(rawLevel: Int): String = "${rawLevelToDecibels(rawLevel)} dB"
