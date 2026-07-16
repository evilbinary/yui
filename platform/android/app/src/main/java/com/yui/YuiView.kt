package com.yui

import android.content.Context
import android.view.Choreographer
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import java.io.File
import java.io.FileOutputStream

class YuiView(context: Context) : SurfaceView(context), SurfaceHolder.Callback, Choreographer.FrameCallback {
    private var running = false
    private var dataDir: File = File(context.filesDir, "yui")
    private var jsonPath: String = File(dataDir, "app/watch-os/app.json").absolutePath
    private var assetsRoot: String = File(dataDir, "app/assets").absolutePath

    init {
        isClickable = true
        holder.addCallback(this)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!running) {
            return super.onTouchEvent(event)
        }

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val index = event.actionIndex
                nativeOnTouch(
                    event.getPointerId(index),
                    event.getX(index),
                    event.getY(index),
                    0
                )
            }
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until event.pointerCount) {
                    nativeOnTouch(
                        event.getPointerId(i),
                        event.getX(i),
                        event.getY(i),
                        1
                    )
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_CANCEL -> {
                val index = event.actionIndex
                nativeOnTouch(
                    event.getPointerId(index),
                    event.getX(index),
                    event.getY(index),
                    2
                )
            }
        }
        return true
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        prepareAssets()
        val density = resources.displayMetrics.density
        nativeInit(holder.surface, jsonPath, assetsRoot, dataDir.absolutePath, density)
        val frame = holder.surfaceFrame
        nativeResize(frame.width(), frame.height())
        running = true
        Choreographer.getInstance().postFrameCallback(this)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        nativeResize(width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        running = false
        Choreographer.getInstance().removeFrameCallback(this)
        nativeShutdown()
    }

    override fun doFrame(frameTimeNanos: Long) {
        if (running) {
            nativeTick()
            Choreographer.getInstance().postFrameCallback(this)
        }
    }

    fun shutdown() {
        running = false
        Choreographer.getInstance().removeFrameCallback(this)
        nativeShutdown()
    }

    private fun prepareAssets() {
        dataDir.mkdirs()
        copyAssetDir("")
    }

    private fun copyAssetDir(dir: String) {
        val children = context.assets.list(dir) ?: return
        for (name in children) {
            val assetPath = if (dir.isEmpty()) name else "$dir/$name"
            val sub = context.assets.list(assetPath)
            if (sub != null && sub.isNotEmpty()) {
                copyAssetDir(assetPath)
            } else {
                context.assets.open(assetPath).use { input ->
                    val outFile = File(dataDir, assetPath)
                    outFile.parentFile?.mkdirs()
                    FileOutputStream(outFile).use { output ->
                        input.copyTo(output)
                    }
                }
            }
        }
    }

    private external fun nativeInit(
        surface: android.view.Surface,
        jsonPath: String,
        assetsPath: String,
        dataRoot: String,
        density: Float
    )
    private external fun nativeResize(width: Int, height: Int)
    private external fun nativeOnTouch(pointerId: Int, x: Float, y: Float, phase: Int)
    private external fun nativeTick()
    private external fun nativeShutdown()

    companion object {
        init {
            System.loadLibrary("yui_android_jni")
        }
    }
}
