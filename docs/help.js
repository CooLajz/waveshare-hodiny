const languageButtons = document.querySelectorAll("[data-language]");

function preferredLanguage() {
  const saved = localStorage.getItem("waveshare-hodiny-language");
  if (saved === "cs" || saved === "en") return saved;
  const language = (navigator.language || "").toLowerCase().split("-")[0];
  return language === "cs" || language === "sk" ? "cs" : "en";
}

function applyLanguage(language) {
  document.documentElement.lang = language;
  document.title = language === "en" ? "Waveshare Hodiny – Help" : "Waveshare Hodiny – Nápověda";
  document.querySelector('meta[name="description"]').content = language === "en"
    ? "Concise help for installing, configuring, controlling and updating Waveshare Hodiny."
    : "Stručná uživatelská nápověda pro instalaci, nastavení, ovládání a aktualizace Waveshare Hodiny.";
  document.querySelectorAll("[data-language-content]").forEach((element) => {
    element.hidden = element.dataset.languageContent !== language;
  });
  document.querySelectorAll("[data-cs][data-en]").forEach((element) => {
    element.textContent = element.dataset[language];
  });
  document.querySelectorAll("[data-href-cs][data-href-en]").forEach((element) => {
    element.setAttribute("href", element.dataset[`href${language === "cs" ? "Cs" : "En"}`]);
  });
  document.querySelectorAll("[data-aria-cs][data-aria-en]").forEach((element) => {
    element.setAttribute("aria-label", element.dataset[`aria${language === "cs" ? "Cs" : "En"}`]);
  });
  languageButtons.forEach((button) => button.setAttribute("aria-pressed", button.dataset.language === language ? "true" : "false"));
}

languageButtons.forEach((button) => button.addEventListener("click", () => {
  localStorage.setItem("waveshare-hodiny-language", button.dataset.language);
  applyLanguage(button.dataset.language);
}));

applyLanguage(preferredLanguage());
