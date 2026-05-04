package com.acousense.di

import android.content.Context
import android.content.SharedPreferences
import androidx.room.Room
import com.acousense.data.db.AppDatabase
import com.acousense.data.db.DailySummaryDao
import com.acousense.data.db.SyncSessionDao
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.android.qualifiers.ApplicationContext
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object AppModule {

    @Provides
    @Singleton
    fun providePrefs(@ApplicationContext context: Context): SharedPreferences =
        context.getSharedPreferences("acousense_prefs", Context.MODE_PRIVATE)

    @Provides
    @Singleton
    fun provideDatabase(@ApplicationContext context: Context): AppDatabase =
        Room.databaseBuilder(context, AppDatabase::class.java, "acousense.db").build()

    @Provides
    fun provideSyncSessionDao(db: AppDatabase): SyncSessionDao = db.syncSessionDao()

    @Provides
    fun provideDailySummaryDao(db: AppDatabase): DailySummaryDao = db.dailySummaryDao()
}
