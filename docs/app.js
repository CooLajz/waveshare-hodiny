const installButton = document.querySelector("#install-button");
const versionLabel = document.querySelector("#firmware-version");
const statusLabel = document.querySelector("#installer-status");

async function prepareInstaller() {
  try {
    const response = await fetch("firmware/manifest.json", { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`manifest HTTP ${response.status}`);
    }

    const manifest = await response.json();
    const build = manifest.builds?.find((item) => item.chipFamily === "ESP32-S3");
    if (!manifest.version || !build || build.parts?.length !== 4) {
      throw new Error("neplatný instalační manifest");
    }

    installButton.setAttribute("manifest", "firmware/manifest.json");
    installButton.setAttribute("ready", "");
    versionLabel.textContent = `Verze ${manifest.version}`;
    statusLabel.textContent = "Firmware je připravený k instalaci.";
  } catch (error) {
    versionLabel.textContent = "Firmware zatím nebyl veřejně vydán";
    statusLabel.textContent = "Stránka je připravená. Instalaci zpřístupní první stabilní GitHub release.";
    statusLabel.classList.add("error");
    console.info("Instalátor není aktivní:", error.message);
  }
}

prepareInstaller();
