package com.acousense.ui.theme

import androidx.compose.material3.Typography
import androidx.compose.runtime.Immutable
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.googlefonts.Font
import androidx.compose.ui.text.googlefonts.GoogleFont
import androidx.compose.ui.unit.sp

private val googleFontsProvider = GoogleFont.Provider(
    providerAuthority = "com.google.android.gms.fonts",
    providerPackage = "com.google.android.gms",
    certificates = com.acousense.R.array.com_google_android_gms_fonts_certs,
)

private val AcouSenseFontFamily = FontFamily(
    Font(GoogleFont("Inter"), googleFontsProvider, FontWeight.W300),
    Font(GoogleFont("Inter"), googleFontsProvider, FontWeight.W400),
    Font(GoogleFont("Inter"), googleFontsProvider, FontWeight.W500),
    Font(GoogleFont("Inter"), googleFontsProvider, FontWeight.W600),
    Font(GoogleFont("Inter"), googleFontsProvider, FontWeight.W700),
)

val AcouSenseTypography = Typography(
    displayLarge = TextStyle(
        fontFamily = AcouSenseFontFamily,
        fontSize = 57.sp,
        fontWeight = FontWeight.W300,
        letterSpacing = (-0.25).sp,
    ),
    displayMedium = TextStyle(
        fontFamily = AcouSenseFontFamily,
        fontSize = 45.sp,
        fontWeight = FontWeight.W400,
    ),
    displaySmall = TextStyle(
        fontFamily = AcouSenseFontFamily,
        fontSize = 36.sp,
        fontWeight = FontWeight.W400,
    ),
    headlineLarge = TextStyle(
        fontFamily = AcouSenseFontFamily,
        fontSize = 32.sp,
        fontWeight = FontWeight.W600,
    ),
    headlineMedium = TextStyle(
        fontFamily = AcouSenseFontFamily,
        fontSize = 28.sp,
        fontWeight = FontWeight.W600,
    ),
    headlineSmall = TextStyle(
        fontFamily = AcouSenseFontFamily,
        fontSize = 24.sp,
        fontWeight = FontWeight.W500,
    ),
    titleLarge = TextStyle(
        fontFamily = AcouSenseFontFamily,
        fontSize = 22.sp,
        fontWeight = FontWeight.W500,
    ),
    titleMedium = TextStyle(
        fontFamily = AcouSenseFontFamily,
        fontSize = 16.sp,
        fontWeight = FontWeight.W600,
        letterSpacing = 0.15.sp,
    ),
    titleSmall = TextStyle(
        fontFamily = AcouSenseFontFamily,
        fontSize = 14.sp,
        fontWeight = FontWeight.W600,
        letterSpacing = 0.1.sp,
    ),
    bodyLarge = TextStyle(
        fontFamily = AcouSenseFontFamily,
        fontSize = 16.sp,
        fontWeight = FontWeight.W400,
        lineHeight = 24.sp,
    ),
    bodyMedium = TextStyle(
        fontFamily = AcouSenseFontFamily,
        fontSize = 14.sp,
        fontWeight = FontWeight.W400,
        lineHeight = 20.sp,
    ),
    bodySmall = TextStyle(
        fontFamily = AcouSenseFontFamily,
        fontSize = 12.sp,
        fontWeight = FontWeight.W400,
        lineHeight = 16.sp,
    ),
    labelLarge = TextStyle(
        fontFamily = AcouSenseFontFamily,
        fontSize = 14.sp,
        fontWeight = FontWeight.W600,
        letterSpacing = 0.1.sp,
    ),
    labelMedium = TextStyle(
        fontFamily = AcouSenseFontFamily,
        fontSize = 12.sp,
        fontWeight = FontWeight.W500,
        letterSpacing = 0.5.sp,
    ),
    labelSmall = TextStyle(
        fontFamily = AcouSenseFontFamily,
        fontSize = 11.sp,
        fontWeight = FontWeight.W500,
        letterSpacing = 0.5.sp,
    ),
)

@Immutable
data class AcouSenseTextStyles(
    val display: TextStyle,
    val title: TextStyle,
    val label: TextStyle,
    val body: TextStyle,
    val caption: TextStyle,
)

val AcouSenseText = AcouSenseTextStyles(
    display = AcouSenseTypography.displayLarge,
    title = AcouSenseTypography.titleLarge,
    label = AcouSenseTypography.labelMedium,
    body = AcouSenseTypography.bodyMedium,
    caption = AcouSenseTypography.labelSmall,
)
