package com.yui

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity

class YuiActivity : AppCompatActivity() {
    private lateinit var yuiView: YuiView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        yuiView = YuiView(this)
        setContentView(yuiView)
    }

    override fun onDestroy() {
        yuiView.shutdown()
        super.onDestroy()
    }
}
