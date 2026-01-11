
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ZONAS 5
#define DIAS_HIST 30


#define LIM_PM25_24H 15.0f
#define LIM_NO2_24H  25.0f
#define LIM_SO2_24H  40.0f

#define DEFAULT_CO2_PPM 1000.0f

typedef struct {
    float tempC;
    float viento_ms;
    float humedad_pct;
} Clima;

typedef struct {
    char nombre[32];

    
    float hist_pm25[DIAS_HIST];
    float hist_no2[DIAS_HIST];
    float hist_so2[DIAS_HIST];
    float hist_co2[DIAS_HIST]; 

  
    float act_pm25;
    float act_no2;
    float act_so2;
    float act_co2;

    Clima clima;
} Zona;


static void limpiarNuevaLinea(char *s) {
    size_t n = strlen(s);
    if (n > 0 && s[n-1] == '\n') s[n-1] = '\0';
}

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}


static void push_hist(float hist[DIAS_HIST], float nuevo) {
    for (int i = DIAS_HIST - 1; i > 0; --i) {
        hist[i] = hist[i-1];
    }
    hist[0] = nuevo;
}


static float promedio30(const float hist[DIAS_HIST]) {
    float s = 0.0f;
    for (int i = 0; i < DIAS_HIST; ++i) s += hist[i];
    return s / (float)DIAS_HIST;
}


static float ponderado3(const float hist[DIAS_HIST]) {
    return 0.5f*hist[0] + 0.3f*hist[1] + 0.2f*hist[2];
}


static float ajuste_clima(float base, Clima c) {
    const float kT = 0.10f;
    const float kV = 0.15f;
    const float kH = 0.05f;

    float dT = (c.tempC - 20.0f) / 20.0f;
    float dV = (c.viento_ms - 2.0f) / 5.0f;
    float dH = (c.humedad_pct - 50.0f) / 50.0f;

    float factor = 1.0f + (kT*dT) - (kV*dV) + (kH*dH);
    /* Evitar factores extremos */
    factor = clampf(factor, 0.70f, 1.50f);
    return base * factor;
}

static int excede(float valor, float limite) {
    return valor > limite;
}


static int guardarCSV(const char *ruta, Zona zonas[], int nZonas) {
    FILE *f = fopen(ruta, "w");
    if (!f) return 0;

    fprintf(f, "zona,dia,pm25,no2,so2,co2\n");
    for (int z = 0; z < nZonas; ++z) {
        for (int d = 0; d < DIAS_HIST; ++d) {
            fprintf(f, "%s,%d,%.3f,%.3f,%.3f,%.3f\n",
                    zonas[z].nombre, d,
                    zonas[z].hist_pm25[d],
                    zonas[z].hist_no2[d],
                    zonas[z].hist_so2[d],
                    zonas[z].hist_co2[d]);
        }
    }
    fclose(f);
    return 1;
}


static int cargarCSV(const char *ruta, Zona zonas[], int nZonas) {
    FILE *f = fopen(ruta, "r");
    if (!f) return 0;

    char linea[256];
    /* Saltar cabecera */
    if (!fgets(linea, sizeof(linea), f)) { fclose(f); return 0; }

    while (fgets(linea, sizeof(linea), f)) {
        char nombre[64];
        int dia;
        float pm25, no2, so2, co2;

        if (sscanf(linea, "%63[^,],%d,%f,%f,%f,%f",
                   nombre, &dia, &pm25, &no2, &so2, &co2) != 6) {
            continue;
        }

        for (int z = 0; z < nZonas; ++z) {
            if (strcmp(nombre, zonas[z].nombre) == 0) {
                if (dia >= 0 && dia < DIAS_HIST) {
                    zonas[z].hist_pm25[dia] = pm25;
                    zonas[z].hist_no2[dia] = no2;
                    zonas[z].hist_so2[dia] = so2;
                    zonas[z].hist_co2[dia] = co2;
                }
                break;
            }
        }
    }

    fclose(f);
    return 1;
}


static void imprimirRecomendaciones(int alertaPM25, int alertaNO2, int alertaSO2, int alertaCO2) {
   
    if (!alertaPM25 && !alertaNO2 && !alertaSO2 && !alertaCO2) {
        printf("  - Mantener medidas preventivas: transporte público, verificación vehicular y monitoreo continuo.\n");
        return;
    }

    printf("  - Activar campaña informativa a la ciudadanía (grupos vulnerables: niños, adultos mayores, asmáticos).\n");
    printf("  - Reforzar transporte público / teletrabajo parcial en horas pico si es viable.\n");

    if (alertaPM25) {
        printf("  - Reducir fuentes de material particulado: control de polvo en vías/obras, limitar quemas, barrido húmedo.\n");
        printf("  - Recomendar suspensión o reprogramación de actividad física intensa al aire libre.\n");
    }
    if (alertaNO2) {
        printf("  - Medidas de reducción de tráfico: restricción temporal, rutas alternas, control de emisiones.\n");
    }
    if (alertaSO2) {
        printf("  - Supervisar/regular emisiones industriales; considerar reducción temporal de operación en fuentes puntuales.\n");
    }
    if (alertaCO2) {
        printf("  - (CO2) Mejorar ventilación y gestión de espacios cerrados si aplica; verificar fuentes de combustión.\n");
    }

    printf("  - Monitoreo reforzado y reevaluación cada 6-12 horas.\n");
}


static void initZonas(Zona zonas[], int *nZonas, float *limCO2) {
    *nZonas = MAX_ZONAS;
    *limCO2 = DEFAULT_CO2_PPM;

    const char *nombres[MAX_ZONAS] = {"Centro", "Norte", "Sur", "Industrial", "Residencial"};
    for (int z = 0; z < *nZonas; ++z) {
        strncpy(zonas[z].nombre, nombres[z], sizeof(zonas[z].nombre));
        zonas[z].nombre[sizeof(zonas[z].nombre)-1] = '\0';

       
        for (int d = 0; d < DIAS_HIST; ++d) {
            zonas[z].hist_pm25[d] = 10.0f + (float)(z);  
            zonas[z].hist_no2[d]  = 15.0f + (float)(z);
            zonas[z].hist_so2[d]  = 20.0f + (float)(z);
            zonas[z].hist_co2[d]  = 800.0f + 10.0f*(float)(z);
        }

        zonas[z].act_pm25 = zonas[z].hist_pm25[0];
        zonas[z].act_no2  = zonas[z].hist_no2[0];
        zonas[z].act_so2  = zonas[z].hist_so2[0];
        zonas[z].act_co2  = zonas[z].hist_co2[0];

        zonas[z].clima.tempC = 20.0f;
        zonas[z].clima.viento_ms = 2.0f;
        zonas[z].clima.humedad_pct = 50.0f;
    }
}



static void mostrarZonas(Zona zonas[], int nZonas) {
    printf("\nZonas disponibles:\n");
    for (int z = 0; z < nZonas; ++z) {
        printf("  [%d] %s\n", z, zonas[z].nombre);
    }
}

static void ingresarLecturas(Zona *pz) {
    printf("\nIngresar lecturas actuales para zona: %s\n", pz->nombre);

    printf("  PM2.5 (ug/m3): "); scanf("%f", &pz->act_pm25);
    printf("  NO2  (ug/m3): "); scanf("%f", &pz->act_no2);
    printf("  SO2  (ug/m3): "); scanf("%f", &pz->act_so2);
    printf("  CO2  (ppm):   "); scanf("%f", &pz->act_co2);

    printf("  Temperatura (C): "); scanf("%f", &pz->clima.tempC);
    printf("  Viento (m/s):    "); scanf("%f", &pz->clima.viento_ms);
    printf("  Humedad (%%):     "); scanf("%f", &pz->clima.humedad_pct);

   
    push_hist(pz->hist_pm25, pz->act_pm25);
    push_hist(pz->hist_no2,  pz->act_no2);
    push_hist(pz->hist_so2,  pz->act_so2);
    push_hist(pz->hist_co2,  pz->act_co2);

    printf("  OK: lectura registrada y agregada al historial.\n");
}

static void imprimirPromedios(Zona zonas[], int nZonas) {
    printf("\nPROMEDIOS HISTORICOS (30 dias):\n");
    for (int z = 0; z < nZonas; ++z) {
        float pPM25 = promedio30(zonas[z].hist_pm25);
        float pNO2  = promedio30(zonas[z].hist_no2);
        float pSO2  = promedio30(zonas[z].hist_so2);
        float pCO2  = promedio30(zonas[z].hist_co2);

        printf("\nZona: %s\n", zonas[z].nombre);
        printf("  PM2.5: %.2f ug/m3\n", pPM25);
        printf("  NO2 :  %.2f ug/m3\n", pNO2);
        printf("  SO2 :  %.2f ug/m3\n", pSO2);
        printf("  CO2 :  %.2f ppm\n",  pCO2);
    }
}

static void predecirYAlertar(Zona zonas[], int nZonas, float limCO2) {
    printf("\nPREDICCION 24H + ALERTAS (basado en ponderado 3 dias + ajuste clima):\n");

    for (int z = 0; z < nZonas; ++z) {
        Zona *pz = &zonas[z];

        float basePM25 = ponderado3(pz->hist_pm25);
        float baseNO2  = ponderado3(pz->hist_no2);
        float baseSO2  = ponderado3(pz->hist_so2);
        float baseCO2  = ponderado3(pz->hist_co2);

        float predPM25 = ajuste_clima(basePM25, pz->clima);
        float predNO2  = ajuste_clima(baseNO2,  pz->clima);
        float predSO2  = ajuste_clima(baseSO2,  pz->clima);
        float predCO2  = ajuste_clima(baseCO2,  pz->clima);

        int aPM25 = excede(predPM25, LIM_PM25_24H);
        int aNO2  = excede(predNO2,  LIM_NO2_24H);
        int aSO2  = excede(predSO2,  LIM_SO2_24H);
        int aCO2  = excede(predCO2,  limCO2);

        printf("\nZona: %s\n", pz->nombre);
        printf("  Pred PM2.5: %.2f (lim %.2f) %s\n", predPM25, LIM_PM25_24H, aPM25 ? "ALERTA" : "OK");
        printf("  Pred NO2 :  %.2f (lim %.2f) %s\n", predNO2,  LIM_NO2_24H,  aNO2  ? "ALERTA" : "OK");
        printf("  Pred SO2 :  %.2f (lim %.2f) %s\n", predSO2,  LIM_SO2_24H,  aSO2  ? "ALERTA" : "OK");
        printf("  Pred CO2 :  %.2f (lim %.2f) %s\n", predCO2,  limCO2,      aCO2  ? "ALERTA" : "OK");

        printf("  Recomendaciones:\n");
        imprimirRecomendaciones(aPM25, aNO2, aSO2, aCO2);
    }
}

static int exportarReporte(const char *ruta, Zona zonas[], int nZonas, float limCO2) {
    FILE *f = fopen(ruta, "w");
    if (!f) return 0;

    fprintf(f, "REPORTE SIGPCA-ZU\n");
    fprintf(f, "Umbrales 24h: PM2.5=%.2f ug/m3, NO2=%.2f ug/m3, SO2=%.2f ug/m3, CO2=%.2f ppm\n\n",
            LIM_PM25_24H, LIM_NO2_24H, LIM_SO2_24H, limCO2);

    for (int z = 0; z < nZonas; ++z) {
        Zona *pz = &zonas[z];

        float predPM25 = ajuste_clima(ponderado3(pz->hist_pm25), pz->clima);
        float predNO2  = ajuste_clima(ponderado3(pz->hist_no2),  pz->clima);
        float predSO2  = ajuste_clima(ponderado3(pz->hist_so2),  pz->clima);
        float predCO2  = ajuste_clima(ponderado3(pz->hist_co2),  pz->clima);

        fprintf(f, "Zona: %s\n", pz->nombre);
        fprintf(f, "Actual: PM2.5=%.2f NO2=%.2f SO2=%.2f CO2=%.2f\n",
                pz->act_pm25, pz->act_no2, pz->act_so2, pz->act_co2);
        fprintf(f, "Pred24h: PM2.5=%.2f NO2=%.2f SO2=%.2f CO2=%.2f\n",
                predPM25, predNO2, predSO2, predCO2);

        fprintf(f, "Prom30d: PM2.5=%.2f NO2=%.2f SO2=%.2f CO2=%.2f\n",
                promedio30(pz->hist_pm25), promedio30(pz->hist_no2),
                promedio30(pz->hist_so2),  promedio30(pz->hist_co2));

        fprintf(f, "Alertas: PM2.5=%s NO2=%s SO2=%s CO2=%s\n",
                excede(predPM25, LIM_PM25_24H) ? "SI" : "NO",
                excede(predNO2,  LIM_NO2_24H)  ? "SI" : "NO",
                excede(predSO2,  LIM_SO2_24H)  ? "SI" : "NO",
                excede(predCO2,  limCO2)       ? "SI" : "NO");

        fprintf(f, "-----\n");
    }

    fclose(f);
    return 1;
}

int main(void) {
    Zona zonas[MAX_ZONAS];
    int nZonas = 0;
    float limCO2 = 0.0f;

    initZonas(zonas, &nZonas, &limCO2);


    cargarCSV("historial.csv", zonas, nZonas);

    int op = 0;
    while (1) {
        printf("\n=== SIGPCA-ZU ===\n");
        printf("1) Ingresar/Actualizar lecturas actuales + clima (por zona)\n");
        printf("2) Calcular y mostrar promedios historicos (30 dias)\n");
        printf("3) Predecir 24h y mostrar alertas + recomendaciones\n");
        printf("4) Configurar umbral CO2 (ppm) [actual: %.2f]\n", limCO2);
        printf("5) Exportar reporte a archivo (reporte.txt)\n");
        printf("6) Guardar y salir\n");
        printf("Opcion: ");
        if (scanf("%d", &op) != 1) {
            printf("Entrada invalida.\n");
            
            int c; while ((c = getchar()) != '\n' && c != EOF) {}
            continue;
        }

        if (op == 1) {
            mostrarZonas(zonas, nZonas);
            int z;
            printf("Seleccione zona (0-%d): ", nZonas-1);
            scanf("%d", &z);
            if (z < 0 || z >= nZonas) {
                printf("Zona invalida.\n");
            } else {
                ingresarLecturas(&zonas[z]);
            }
        } else if (op == 2) {
            imprimirPromedios(zonas, nZonas);
        } else if (op == 3) {
            predecirYAlertar(zonas, nZonas, limCO2);
        } else if (op == 4) {
            printf("Nuevo umbral CO2 (ppm): ");
            scanf("%f", &limCO2);
            if (limCO2 < 400.0f) limCO2 = 400.0f;
            printf("OK: Umbral CO2 configurado en %.2f ppm\n", limCO2);
        } else if (op == 5) {
            if (exportarReporte("reporte.txt", zonas, nZonas, limCO2)) {
                printf("OK: reporte.txt generado.\n");
            } else {
                printf("Error: no se pudo generar reporte.\n");
            }
        } else if (op == 6) {
            if (guardarCSV("historial.csv", zonas, nZonas)) {
                printf("OK: historial.csv guardado.\n");
            } else {
                printf("Error: no se pudo guardar historial.csv\n");
            }
            printf("Saliendo...\n");
            break;
        } else {
            printf("Opcion no valida.\n");
        }
    }

    return 0;
}
