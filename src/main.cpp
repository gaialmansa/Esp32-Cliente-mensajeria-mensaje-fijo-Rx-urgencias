
#include <beeper.h>
#include <tft.h>
#include <WiFibeeper.h>

void setup() 
{
    String mensaje;
    #ifdef _DEBUG
    Serial.begin(9600);
    #endif
    // Inicializar pantalla
    iniScreen();
    // inicializamos wifi
    WiFiStart();
    
    // Puesta en hora
    timeClient.setTimeOffset(3600);
    timeClient.begin();
    if (!timeClient.update()) 
      tftPrint("Error al sincronizar NTP: SSL/conexion fallida");
    mensaje = "Ajustando hora:" + strNow();

    // Imprimimos la MAC en la pantalla
    cls();
    tft.println("MAC: ");
    tftPrint(WiFi.macAddress());
    
    #ifdef _DEBUG
    Serial.println(mensaje);
    #endif
    apicall.setTimeout(5000);
    regSys();     // Chequeamos hasta que esta registrado en el sistema
    #ifdef _DEBUG
     delay(5000); // esperamos 1 segundos para que se vea el registro correcto
    #endif
    cls();       // y borramos la pantalla 

    // configuracion de los pines de entrada
    pinMode(UP_PIN, INPUT_PULLUP);
    pinMode(DOWN_PIN, INPUT_PULLUP);
    pinMode(LEFT_PIN, INPUT_PULLUP);
    pinMode(RIGHT_PIN, INPUT_PULLUP);
    pinMode(PUSH_PIN, INPUT_PULLUP);
  
    // Configuracion de las interrupciones. De entrada solo puede usar el pulsador
    attachInterrupt(PUSH_PIN, pushISR, FALLING);

    // Configurar el temporizador para despertar en 5 segundos
    esp_sleep_enable_timer_wakeup(5 * 1000000);
    // Configurar la interrupción del botón para despertar el ESP32
    //esp_sleep_enable_ext0_wakeup(WAKEPIN, 0); // 0 para FALLING, 1 para RISING
    //gpio_wakeup_enable(GPIO_NUM_3, GPIO_INTR_LOW_LEVEL); // x es el pin que quieres usar
    esp_sleep_enable_gpio_wakeup();
}
void loop() 
{
   chkMsg();    // consulta la API para ver si hay mensajes nuevos para el usuario
   dormir();  // apagamos la pantalla y ponemos el procesador en modo light sleep durante 5 segundos
}
/**
 *  chkMsg()
 *  A esta funcion se le llama cada 5 segundos si está inactivo. Consulta el API para ver si hay mensajes no vistos para el usuario.
 *  
 */
void chkMsg()
{
  String User[1];
  User[0] = "id_usuario=" + String(user);
  while (true)
  {
  Api("mnv",User,1);  // llamamos al api para recuperar el primer mensaje sin leer del usuario
  if(!doc.containsKey("mensaje")) // esto es que no hay ningun mensaje
    return;
  hayMensajeNuevo();
  }
}
/**
 *  hayMensajeNuevo
 *  enciende la pantalla y enseña el mensaje que se acaba de leer. Al terminar lo marca en la API como leido
 *  Se queda en bucle hasta que se pulsa el joystick
 */
void hayMensajeNuevo()
{
  int anterior;
  despertarTFT();  //despierta la pantalla
  cls();
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString("NUEVO MENSAJE",0,0);
  tft.setTextSize(1);
  tft.drawFastHLine(0, 17, 160, TFT_YELLOW);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0,30);
  tft.print("De ");
  tftPrint(doc["origen"].as<String>());
  tftPrint(doc["mensaje"].as<String>());
  tft.print("Enviado a las ");
  String timestamp = doc["hora"].as<String>();
  tftPrint(timestamp.substring(11,19));
  tft.print("Recibido a las ");
  tft.println(strNow());
  tft.println();
  aiMenu(opt);
  anterior =0;  // opcion anteriormente seleccionada
  pulsado = false;
  attachInterrupt(PUSH_PIN, pushISR, FALLING);    // pulsador
  attachInterrupt(UP_PIN, upISR, FALLING);
  attachInterrupt(DOWN_PIN, downISR, FALLING);

  while(!pulsado) // se queda bloqueado aqui hasta que pulse el mando
  {
    if (opt != anterior)  // se pulso arriba o abajo
    {
      aiMenu(opt);
      anterior = opt;
    } 
    delay(100);
  }
  String parms[1];
  parms[0] = "id=" + doc["id"].as<String>();
  Api("mver",parms,1);
  if(opt == 0) // aceptar
   Api("matender",parms,1);   // si se ignora, no hace nada
  cls();
  #ifdef _DEBUG
  Serial.printf("Seleccionado opcion %d\n",opt);
  #endif
  
}
void aiMenu(int opcion)
{
  if(opcion == 0)
  {
    tft.fillRect(0,109,54,8,TFT_WHITE); // fondo blanco
    tft.setTextColor(TFT_BLACK);
    tft.drawString("<Aceptar>",0,109);
    tft.fillRect(0,118,54,8,TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("<Ignorar>",0,118);
  }
  else
  {
    tft.fillRect(0,109,54,8,TFT_BLACK); // fondo negro
    tft.drawString("<Aceptar>",0,109);
    tft.fillRect(0,118,54,8,TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.drawString("<Ignorar>",0,118);
    tft.setTextColor(TFT_WHITE);
  }
  
}
void dormir()
{
  digitalWrite(TFT_BL, LOW); // poniendolo en low, apagamos la pantalla
  tft.writecommand(TFT_DISPOFF);    // Apagar la pantalla
  tft.writecommand(TFT_SLPIN);      // Poner controlador en modo sueño
  // Primero desconectamos WiFi pero mantenemos la configuración
  WiFi.disconnect(false);
  delay(100);
  esp_wifi_stop();
  Serial.println("WiFi apagado");
  Serial.flush();
  esp_light_sleep_start();          // ponemos el ESP32 en light sleep para 5 segundos. En el setup hemos configurado que despierte al pulsar el boton del joystick
  delay(100);
  Serial.println("despertado.");
  // Al despertar, reiniciamos el WiFi
  esp_wifi_start();
  delay(100);
  // Reconectamos
  WiFi.begin(ssid, pass);
  // Esperamos a la conexión con timeout
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 5000) 
  {
      delay(100);
  }
}

void flechasEnable()
{
    attachInterrupt(UP_PIN, upISR, FALLING);
    attachInterrupt(DOWN_PIN, downISR, FALLING);
    attachInterrupt(LEFT_PIN, leftISR, FALLING);
    attachInterrupt(RIGHT_PIN, rightISR, FALLING);
  
}
void flechasDisable()
{
    detachInterrupt(UP_PIN);
    detachInterrupt(DOWN_PIN);
    detachInterrupt(LEFT_PIN);
    detachInterrupt(RIGHT_PIN);
  
}
/**
 * ObtenerMensajes
 * recupera los (maximo 8) últimos mensajes, dependiendo de la variable global offset
 */
void obtenerMensajes()
{
 //tft.setFreeFont(&Salmon_Season20pt7b);
 tft.setTextSize(2);
 tft.drawString(" ULTIMOS MENSAJES",0,0);
 //tft.setFreeFont(&Salmon_Season12pt7b); 
 tft.setTextSize(1);
 tft.drawFastHLine(1, 33, 320, TFT_WHITE);
 tft.setCursor(0,55);



 String parametros[3]; // Primero declaramos el array con tamaño fijo
 parametros[0] = "id_usuario=" + String(user);
 parametros[1] = "nmensajes=" + String(nmensajes);
 parametros[2] = "offset=" + String(offset);
 Api("mrecuperar",parametros,3);
 JsonArray listaMensajes = doc.as<JsonArray>(); // recuperamos la respuesta
 for (int i = 0; i < listaMensajes.size(); i++) 
 {
  JsonObject mensaje = listaMensajes[i].as<JsonObject>();
  tft.println(mensaje["mensaje"].as<String>());
 }
 return ;
}
/**
 *  regSys
 *  Registra al usuario en el sistema. Se llama a la API, al metodo bregister(mac).
 *  Se mantiene la llamada en un bucle hasta que el usuario es no nulo. Entonces regresa y se ejecuta loop.
 */
void regSys()
{
  String nombre,usuario = "null";
  String mac[1];
  mac[0] = "mac=";
  mac[0] += WiFi.macAddress();
  tftPrint("Registrando el dispositivo ");
  tft.println(mac[0]);
  while (usuario == "null") // repetiremos hasta que el usuario tenga un valor
  {
    Api("bregister",mac,1);
    usuario = doc["usuario"].as<String>();
    nombre = doc["nombre" ].as<String>();
    user = doc["id_usuario"].as<int>();
    #ifdef _DEBUG
    Serial.println("No registrado.");
    #endif
    delay(1000);  // esperamos un segundo
  }
  tftPrint("**");
  tft.println();
  tftPrint("Registrado para el usuario " + nombre);
  #ifdef _DEBUG
  Serial.printf("Registrado para el usuario %d\n %s", user, nombre.c_str());
  #endif
}

/**
 * Api
 * Hace una llamada al metodo del Api indicado con los parametros que van en el array
 * Los parametros van en formato 'paramname=paramvalue'
 */
void Api(char metodo[], String parametros[],int numparam)
{
   int f, responsecode;
  String postData, url, payload;
  DeserializationError error;   // por si esta mal formada la respuesta

  postData = parametros[0];
  if(numparam >1)
  {
    for (f = 1; f < numparam; f++) // concatenamos los elementos pasados en parametros en el formato parm1=valor1&param2=valor2....
      {
        postData += "&";
        postData += parametros[f];
      }
  }
  url = _URL;
  url += metodo; // con esto queda formada la url del API con su metodo al final
  #ifdef _DEBUG
  Serial.println(postData);
  Serial.println(url);
  #endif
  apicall.begin(url);    // iniciamos la llamada al api
  // Especificamos el header content-type
  apicall.addHeader("Content-Type", "application/x-www-form-urlencoded");
  responsecode = apicall.POST(postData);  
    if(responsecode != 200)
    {
      tft.printf("Ha habido un error %d al comunicar con el api.\n",responsecode);
      tft.println(url);
      tft.println(postData);
      #ifdef _DEBUG
      Serial.printf("Ha habido un error %d al comunicar con el api.\n",responsecode);
      apiError = true;
      #endif

    }
  payload = apicall.getString();
  error = deserializeJson(doc,payload);   // deserializamos la respuesta y la metemos en el objeto doc
  #ifdef _DEBUG
  Serial.print("payload: ");
  Serial.println(payload);
    if (error) 
    {
    tft.print("Error al parsear JSON: ");
    tft.println(error.f_str());
    Serial.print("Error al parsear JSON: ");
    Serial.println(error.f_str());
    Serial.println(payload);
    return;
    }
  #endif
}
/**
 *  Envía un mensaje fijo. Esta funcion es para estar en sitios como la bandeja de Rx del control de enfermeria o en triaje
 *  TODO: hay que sacar la comunicacion con el cliente http y abstraerla en una funcion a que tome la URL de un #define en una cabecera
 *        y se pase el método del API  y los argumentos como parámetro, y que devuelva la respuesta del API como un array.
 */
void enviarPlaca()
{
    HTTPClient http;   
    // Iniciamos la conexión
    String mensaje;
    mensaje = replaceSpaces("Hay una petición de rayos en el control de urgencias"); 
    http.begin("https://hospital.almansa.ovh/api/mcrearg");
    // Especificamos el header content-type
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    // Preparamos los datos POST
    String postData = "id_grupo=4&id_usuario_o=1&mensaje=" + mensaje;

    // Hacemos la petición POST
    int httpResponseCode = http.POST(postData);
    // Verificamos la respuesta
    if(httpResponseCode < 0)
        tft.print("Error enviando mensaje");
    else
        {
        tft.setTextSize(2);
        cls();
        if (httpResponseCode == 200)
        {
            tft.println("Mensaje enviado");
            tft.println("a las " + strNow());
            Serial.println(httpResponseCode);
        }
        }

}
/*
*   Recupera la lista de usuarios de un grupo dado
*/
void recuperarUsuariosGrupo(int grupo)
{
 HTTPClient http;
 int f;    
    // Iniciamos la conexión
    http.begin("https://hospital.almansa.ovh/api/recuperarusuariosgrupo");
    //http.begin("http://192.168.10.152/mensajeria/api/recuperarusuariosgrupo");
    // Especificamos el header content-type
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    // Preparamos los datos POST
    String postData = "id_grupo="+ String(grupo); 
    // Hacemos la petición POST
    int httpResponseCode = http.POST(postData);
    // Verificamos la respuesta
    if(httpResponseCode > 0)
    {
      String response = http.getString();
      cls();
      tft.setTextFont(2);
      tft.println(response);
      #ifdef _DEBUG
      Serial.println(response);
      #endif
      const size_t capacity = JSON_ARRAY_SIZE(10) * (JSON_OBJECT_SIZE(7) + 50); // Ajustar el tamaño según tu JSON
      DynamicJsonDocument doc(capacity);
      DeserializationError error = deserializeJson(doc, response);
      #ifdef _DEBUG
      Serial.println("Ya ha desserializado la respuesta");
      #endif
        if(!error)
        {
            JsonArray usuarios = doc.as<JsonArray>();
            #ifdef _DEBUG
            Serial.println("No error. ");
            Serial.println(usuarios.size());
            #endif
            for (JsonVariant usuario : usuarios) 
            {
                #ifdef _DEBUG
                Serial.println("Estamos en el bucle");
                #endif
                String id_grupo = usuario["id_grupo"];
                String id_usuario = usuario["id_usuario"];
                String id = usuario["id"];
                String usuario_nombre = usuario["usuario"];
                String nombre = usuario["nombre"];
                String observaciones = usuario["observaciones"];
                String grupo = usuario["grupo"];
                #ifdef _DEBUG
                Serial.print("ID Grupo: ");
                Serial.println(id_grupo);
                Serial.println(nombre);
                #endif
                tft.println(nombre);
                // ... (Imprimir otros campos)
            }
        }
        else
        {
            tft.println("Se ha producido un error");
            #ifdef _DEBUG
            Serial.println("Se ha producido un error");
            Serial.println(response);
            #endif
        }
    }
    else 
    {
      #ifdef _DEBUG
      Serial.println("Error en petición HTTP: " + String(httpResponseCode));
       #endif
      tft.println("Error en petición HTTP: " + String(httpResponseCode));
    }
    
    // Liberamos recursos
    http.end();
}
/*
*   Reemplaza los espacios en str por %20
*/
String replaceSpaces(String str) 
{
  String result = "";
  for (int i = 0; i < str.length(); i++) {
    if (str[i] == ' ') {
      result += "%20";
    } else {
      result += str[i];
    }
  }
  return result;
}
/**
 *  Recupera la hora actual y devuelve un string para imprimir
 */
String strNow()
{
  timeClient.update();

  // Obtener la hora en formato Unix
  unsigned long epochTime = timeClient.getEpochTime();

  // Convertir a una estructura de tiempo
  struct tm *ptm = gmtime((time_t *)&epochTime);

  // Formatear la hora como un string
  char formattedTime[32];
  sprintf(formattedTime, "%02d:%02d:%02d", ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
return(formattedTime);
}
/**
 * ISRs para el joystick
 */
void IRAM_ATTR upISR() 
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime[0] > debounceDelay) 
  {
    lastDebounceTime[0] = currentTime;
    opt --;
    if (opt<0)
      opt=0;
  }
}

void IRAM_ATTR downISR() 
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime[1] > debounceDelay) 
  {
    lastDebounceTime[1] = currentTime;
    opt ++;
    if(opt>totalOpciones)
      opt = totalOpciones;
  }
}

void IRAM_ATTR leftISR() 
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime[2] > debounceDelay) 
  {
    lastDebounceTime[2] = currentTime;
  }
}

void IRAM_ATTR rightISR() 
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime[3] > debounceDelay) 
  {
    lastDebounceTime[3] = currentTime;
  }
}

void IRAM_ATTR pushISR() 
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime[4] > debounceDelay) 
  {
    pulsado = true;
    lastDebounceTime[4] = currentTime;
  }

}
