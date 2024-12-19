#include "HTTP-Server.h"
#include "Melody/Melody.h"

HTTP_Server::HTTP_Server()
    : server(80), priceInput("0"), digits(250), displayEnabled(false), buzzerActive(false) {}

void HTTP_Server::begin()
{
     // Starte einen eigenen Access Point mit festgelegtem Namen und Passwort
    const char* ssid = "NixieClock";
    const char* password = "Bohrmann<3";
    WiFi.softAP(ssid, password);

    // Debug-Informationen
    Serial.println("Access Point gestartet.");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("Passwort: ");
    Serial.println(password);
    Serial.print("IP-Adresse: ");
    Serial.println(WiFi.softAPIP());

    // HTTP-Routen definieren
    server.on("/", [this]() { handleRoot(); });
    server.on("/setPrice", HTTP_POST, [this]() { handleSetPrice(); });
    server.on("/toggleDisplay", HTTP_POST, [this]() { handleToggleDisplay(); });
    server.on("/buzzer", HTTP_POST, [this]() { handleBuzzer(); });

    // HTTP-Server starten
    server.begin();
    Serial.println("HTTP-Server gestartet.");
}

void HTTP_Server::handleClient()
{
    server.handleClient();
}

int32_t HTTP_Server::getDigits() const
{
    return digits;
}

bool HTTP_Server::isDisplayEnabled() const
{
    return displayEnabled;
}

bool HTTP_Server::isBuzzerActive() const
{
    return buzzerActive;
}

void HTTP_Server::handleRoot()
{
    String html = R"(
        <!DOCTYPE html>
        <html>
        <head>
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <style>
                body { font-family: Arial, sans-serif; text-align: center; padding: 20px; }
                button { padding: 10px 20px; font-size: 16px; margin: 10px; cursor: pointer; }
                input { padding: 10px; font-size: 16px; width: 50%; }
            </style>
        </head>
        <body>
            <h1>Nixie Clock Control</h1>
            <form action="/setPrice" method="post">
                <input type="text" name="price" placeholder="Preis eingeben (z.B., 2.50) " required>
                <button type="submit">Preis setzen</button>
            </form>
            <form action="/toggleDisplay" method="post">
                <button type="submit">Roehre ON/OFF</button>
            </form>
            <form action="/buzzer" method="post">
                <button type="submit">Buzzer</button>
            </form>
        </body>
        </html>
    )";
    server.send(200, "text/html", html);
}

void HTTP_Server::handleSetPrice()
{
    if (server.hasArg("price"))
    {
        priceInput = server.arg("price");
        priceInput.replace(",", "");
        priceInput.replace(".", "");
        priceInput.replace(" ", "");

        bool validInput = true;
        for (char c : priceInput)
        {
            if (!isDigit(c))
            {
                validInput = false;
                break;
            }
        }

        if (validInput && priceInput.length() <= 4)
        {
            digits = priceInput.toInt();
            displayEnabled = true;
            server.send(200, "text/html", "Price updated to: " + priceInput + "<br><a href='/'>Back</a>");
        }
        else
        {
            server.send(400, "text/html", "Invalid input format. Use formats like 2,50 / 2.50 / 2 50.<br><a href='/'>Back</a>");
        }
    }
    else
    {
        server.send(400, "text/html", "No price provided.<br><a href='/'>Back</a>");
    }
}

void HTTP_Server::handleToggleDisplay()
{
    displayEnabled = !displayEnabled;
    server.send(200, "text/html", String("Display is now ") + (displayEnabled ? "ON" : "OFF") + ". <a href='/'>Back</a>");
}

void HTTP_Server::handleBuzzer()
{
    buzzerActive = true;
    playGongTone();
    server.send(200, "text/html", "Buzzer activated. <a href='/'>Back</a>");
    buzzerActive = false; // Zurücksetzen, um wieder verwendbar zu sein
}
