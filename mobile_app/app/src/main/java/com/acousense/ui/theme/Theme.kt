package com.acousense.ui.theme

import android.os.Build
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.remember
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext

@Immutable
data class AcouSenseColors(
    val bgBase: Color,
    val bgSurface: Color,
    val bgElevated: Color,
    val accentSafe: Color,
    val accentWarn: Color,
    val accentDanger: Color,
    val accentPrimary: Color,
    val accentAiStart: Color,
    val accentAiEnd: Color,
    val textPrimary: Color,
    val textSecondary: Color,
    val textMuted: Color,
) {
    val accentAiBrush: Brush
        get() = Brush.linearGradient(listOf(accentAiStart, accentAiEnd))
}

val LocalAcouSenseColors = staticCompositionLocalOf {
    AcouSenseColors(
        bgBase = AcouSenseBackground,
        bgSurface = AcouSenseBackground,
        bgElevated = AcouSenseBackground,
        accentSafe = NoiseNone,
        accentWarn = NoiseModerate,
        accentDanger = NoiseHigh,
        accentPrimary = AcouSenseSeed,
        accentAiStart = AcouSenseSeed,
        accentAiEnd = AcouSenseSeed,
        textPrimary = Color.Unspecified,
        textSecondary = Color.Unspecified,
        textMuted = Color.Unspecified,
    )
}

private val FallbackDarkColorScheme = darkColorScheme(
    primary = Color(0xFF9B93FF),
    onPrimary = Color(0xFF1A1560),
    primaryContainer = Color(0xFF2A2470),
    onPrimaryContainer = Color(0xFFCDC8FF),
    secondary = Color(0xFF7A74C9),
    secondaryContainer = Color(0xFF1E1A54),
    onSecondaryContainer = Color(0xFFB8B4FF),
    background = Color(0xFF0B0B12),
    surface = Color(0xFF0F0F1A),
    surfaceVariant = Color(0xFF1A1A2E),
    surfaceContainerLowest = Color(0xFF090910),
    surfaceContainerLow = Color(0xFF0F0F1A),
    surfaceContainer = Color(0xFF141424),
    surfaceContainerHigh = Color(0xFF1A1A2E),
    surfaceContainerHighest = Color(0xFF202038),
    onBackground = Color(0xFFEAE8FF),
    onSurface = Color(0xFFEAE8FF),
    onSurfaceVariant = Color(0xFF8A88AA),
    outline = Color(0xFF4A4870),
    outlineVariant = Color(0xFF2A2850),
)

@Composable
private fun rememberLegacyColors(colorScheme: ColorScheme): AcouSenseColors {
    return remember(colorScheme) {
        AcouSenseColors(
            bgBase = colorScheme.background,
            bgSurface = colorScheme.surfaceContainerLow,
            bgElevated = colorScheme.surfaceContainerHigh,
            accentSafe = NoiseNone,
            accentWarn = NoiseModerate,
            accentDanger = NoiseHigh,
            accentPrimary = colorScheme.primary,
            accentAiStart = colorScheme.primary,
            accentAiEnd = colorScheme.secondary,
            textPrimary = colorScheme.onBackground,
            textSecondary = colorScheme.onSurfaceVariant,
            textMuted = colorScheme.outlineVariant,
        )
    }
}

@Composable
fun AcouSenseTheme(content: @Composable () -> Unit) {
    val context = LocalContext.current
    val colorScheme = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        dynamicDarkColorScheme(context)
    } else {
        FallbackDarkColorScheme
    }

    MaterialTheme(
        colorScheme = colorScheme,
        typography = AcouSenseTypography,
        shapes = AcouSenseShapes,
    ) {
        CompositionLocalProvider(
            LocalAcouSenseColors provides rememberLegacyColors(MaterialTheme.colorScheme),
        ) {
            content()
        }
    }
}
