package io.github.cylear.idolypride.addon

import android.os.Bundle
import androidx.activity.ComponentActivity
import io.github.cylear.idolypride.addon.models.IdolyprideConfig
import io.github.cylear.idolypride.addon.models.ProgramConfig


class TranslucentActivity : ComponentActivity(), IConfigurableActivity<TranslucentActivity> {
    override lateinit var config: IdolyprideConfig
    override lateinit var programConfig: ProgramConfig

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        loadConfig()
        val requestData = intent.getStringExtra("gkmsData")
        if (requestData != null) {
            if (requestData == "requestConfig") {
                onClickStartGame()
                finish()
            }
        }
    }
}
