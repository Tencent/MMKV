import java.net.HttpURLConnection
import java.net.URI
import java.net.URLEncoder
import java.nio.charset.StandardCharsets
import java.util.Base64

/**
 * Gradle's maven-publish plugin only performs Maven-style PUT requests, so the
 * Central OSSRH Staging API cannot infer when a deployment is complete.
 * Sonatype requires a same-IP POST after the final upload to make the
 * deployment visible in Central Portal.
 */

val centralPortalNamespace = providers.gradleProperty("CENTRAL_PORTAL_NAMESPACE")
    .orElse(providers.gradleProperty("GROUP"))
    .orElse("com.tencent")
val centralPortalBaseUrl = providers.gradleProperty("CENTRAL_PORTAL_STAGING_API_URL")
    .orElse("https://ossrh-staging-api.central.sonatype.com")
val centralPortalPublishingType = providers.gradleProperty("CENTRAL_PORTAL_PUBLISHING_TYPE")
    .orElse("user_managed")
val centralPortalDryRun = providers.gradleProperty("CENTRAL_PORTAL_DRY_RUN")
    .map(String::toBoolean)
    .orElse(false)
val centralPortalUsername = providers.gradleProperty("SONATYPE_NEXUS_USERNAME")
    .orElse(providers.gradleProperty("mavenCentralUsername"))
    .orElse(providers.gradleProperty("REPOSITORY_USERNAME"))
val centralPortalPassword = providers.gradleProperty("SONATYPE_NEXUS_PASSWORD")
    .orElse(providers.gradleProperty("mavenCentralPassword"))
    .orElse(providers.gradleProperty("REPOSITORY_PASSWORD"))
val centralPortalRemotePublishTasks = tasks.matching {
    it.name.startsWith("publish") &&
        it.name.endsWith("ToSonatypeRepository") &&
        it.name != "publishAllPublicationsToSonatypeRepository"
}

val uploadDefaultRepositoryToCentralPortal = tasks.register("uploadDefaultRepositoryToCentralPortal") {
    group = "publishing"
    description = "Transfers the completed OSSRH Staging API repository to Central Portal."

    onlyIf {
        !project.version.toString().endsWith("-SNAPSHOT") &&
            centralPortalRemotePublishTasks.toList().all { it.state.failure == null }
    }

    doLast {
        val username = centralPortalUsername.orNull
            ?: error("Missing SONATYPE_NEXUS_USERNAME, mavenCentralUsername, or REPOSITORY_USERNAME.")
        val password = centralPortalPassword.orNull
            ?: error("Missing SONATYPE_NEXUS_PASSWORD, mavenCentralPassword, or REPOSITORY_PASSWORD.")
        val namespace = centralPortalNamespace.get()
        val publishingType = centralPortalPublishingType.get()
        require(publishingType in setOf("user_managed", "automatic", "portal_api")) {
            "Invalid CENTRAL_PORTAL_PUBLISHING_TYPE '$publishingType'. " +
                "Expected user_managed, automatic, or portal_api."
        }

        val encodedNamespace = URLEncoder.encode(namespace, StandardCharsets.UTF_8)
        val encodedType = URLEncoder.encode(publishingType, StandardCharsets.UTF_8)
        val endpoint = "${centralPortalBaseUrl.get()}/manual/upload/defaultRepository/" +
            "$encodedNamespace?publishing_type=$encodedType"

        if (centralPortalDryRun.get()) {
            logger.lifecycle("Central Portal dry run: POST $endpoint")
            return@doLast
        }

        val bearer = Base64.getEncoder().encodeToString(
            "$username:$password".toByteArray(StandardCharsets.UTF_8)
        )
        val connection = URI(endpoint).toURL().openConnection() as HttpURLConnection
        connection.requestMethod = "POST"
        connection.setRequestProperty("Authorization", "Bearer $bearer")
        connection.setRequestProperty("Accept", "application/json")
        connection.connectTimeout = 30_000
        connection.readTimeout = 10 * 60_000

        try {
            val status = connection.responseCode
            val response = (if (status >= 400) connection.errorStream else connection.inputStream)
                ?.bufferedReader(StandardCharsets.UTF_8)
                ?.use { it.readText() }
                .orEmpty()
            check(status in 200..299) {
                "Central Portal handoff failed with HTTP $status" +
                    if (response.isNotBlank()) ": $response" else "."
            }
            logger.lifecycle("Transferred '$namespace' to Central Portal ($publishingType).")
            if (response.isNotBlank()) {
                logger.info("Central Portal response: $response")
            }
        } finally {
            connection.disconnect()
        }
    }
}

tasks.matching {
    it.name == "publishAllPublicationsToSonatypeRepository" || it.name == "publish"
}.configureEach {
    finalizedBy(uploadDefaultRepositoryToCentralPortal)
}
