package io.github.cylear.idolypride.addon.models

import kotlinx.serialization.Serializable

@Serializable
data class AboutPageConfig(
    val plugin_repo: String = "https://github.com/cylear/idolypride-addon",
    val main_contributors: List<MainContributors> = listOf(),
    val contrib_img: ContribImg = ContribImg(
        "https://contrib.rocks/image?repo=cylear/idolypride-addon",
        "https://contrib.rocks/image?repo=cylear/IdolyprideTranslationData"
    )
)

@Serializable
data class MainContributors(
    val name: String,
    val links: List<Links>
)

@Serializable
data class ContribImg(
    val plugin: String,
    val translation: String
)

@Serializable
data class Links(
    val name: String,
    val link: String
)
