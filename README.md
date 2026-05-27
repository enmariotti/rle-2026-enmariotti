
<center>
    <h1>Programación 2</h1>
    <h3>Trabajo práctico integrador</h3>
    <h5>Mayo de 2026</h5>
</center>


A continuación se presentan una serie de opciones para desarrollar un trabajo práctico:

- **Opción 1**: Gestionador y clasificador de logs.
- **Opción 2**: Sugeridor de amistades.
- **Opción 3**: Compresor de imágenes.

*(Ver enunciados más abajo)*

Deberán elegir una de estas opciones, o proponer un proyecto que les interese realizar.

En caso de proponer un proyecto, se debe acordar con la cátedra el alcance y la especificación del problema antes de comenzar a trabajar.

## Pautas de aprobación del trabajo

1. El trabajo es para desarrollar individualmente o en parejas elegidas por cada uno.
2. El entregable del trabajo debe contener:
   1. Código fuente de la aplicación.
   2. Ejecutables.
   3. Casos de prueba.
   4. Documentación del desarrollo:
      1. Diseño de las estructuras de datos. Justificación de la elección.
      2. Reporte de casos de prueba.
3. Cronograma:
   1. El enunciado del trabajo se entregará el día 11/5/2026.
   2. La semana del 25/5, los equipos deberán mostrar el avance con información de:
      1. Especificación del problema.
      2. Estructuras de datos elegidas.
4. Forma de evaluación:
   1. Será mediante un coloquio durante la semana del 8/6.
      1. Serán al menos 20 minutos, con el trabajo y los entregables en mano. 
      2. Se ejecutará el programa y se revisará la documentación.
      3. Se ejecutará un caso de prueba sorpresa definido por la cátedra.
   2. Criterios de evaluación:
      1. Documentación a entregar correcta.
      2. El programa DEBE compilar y ejecutarse sin fallas.
      3. El programa DEBE hacer una correcta gestión de los recursos (memoria, procesador, archivos).
   
---

A continuación se describen los problemas propuestos por la cátedra:
  
## Sugeridor de amistades

Desarrollar un programa que pueda sugerir personas cercanas a un usuario en una red social.

El programa debe leer de un archivo un grafo que representa las conexiones entre los usuarios de la red social.

*Formato de archivo sugerido: CSV o JSON. Ejemplo CSV: `usuario1,usuario2,tipo_relacion`.*

La solución debe modelar a las personas y la relación entre ellas.

Se debe modelar relaciones de amistad, parentesco y sentimentales.

El programa debe tener al menos las siguientes funcionalidades:
- Recibir el nombre de un usurario (el "target"), y mostrar una lista de otros usuarios que no tienen una conexión directa con el target, pero sí tienen conexión indirecta cercana (2 saltos).
- Recibir 2 personas y devolver el grado de cercanía (el camino entre relaciones que lleva de uno al otro).
- Listar los amigos de una persona.
- Las relaciones pueden romperse o cambiar.
- Debe poder gestionar una cantidad grande de usuarios (más de 1000).

## Compresor de imágenes

Desarrollar un programa que pueda comprimir y descomprimir imágenes:

El programa debe tener al menos las siguientes funcionalidades:
- Compresión: El programa recibe un archivo con una imagen en un formato estándar, y genera un nuevo archivo de tamaño menor o igual al original. 
- Debe implementar un formato comprimido propio definido por el equipo. 
- Descompresión: El programa recibe un archivo con una imagen en el formato propio, y genera un archivo en el formato estándar. El archivo descomprimido debe ser exactamente igual al que se comprimió originalmente.
- La solución debe enfocarse en el caso de las imágenes que tienen fondo plano o transparente (osea que hay muchos pixeles que tienen el mismo valor).
- El programa debe poder manejar imágenes de tamañano grande (más de 1024x1024 y hasta 2048x2048).
- El programa debe tardar un tiempo del orden de los segundos o minutos (< 5 minutos).

**Sugerencias:**

* Para el formato estándar, usar GIF o BMP. Estos formatos son los más fáciles de procesar.
* Para el formato comprimido, usar un sistema de [run length encoding](https://es.wikipedia.org/wiki/Run-length_encoding), que es una de las formas más básicas de compresión.

## Procesador de logs

Desarrollar un programa que permita gestionar, analizar y clasificar logs de acceso de un servidor web. El sistema deberá procesar archivos de log generados por un servidor web y brindar información estadística y clasificada a partir de dichos registros. 

Cada línea del archivo de log representa una petición HTTP y contiene, al menos, la siguiente información:

- Dirección IP del cliente
- Fecha y hora de la petición
- Recurso solicitado
- Código de estado HTTP
- Tamaño de la respuesta (opcional)

- El formato exacto del archivo de log será el defindo por ```Apache Combined Log```:

```
  IP - - [DD/MM/AAAA HH:MM:SS] "MÉTODO /recurso HTTP/1.1" CÓDIGO TAMAÑO
```

El programa debe tener al menos las siguientes funcionalidades:
- Cargar logs desde un archivo, validar la estructura y almacenar la información en estructuras de datos adecuadas.
- Informar estadísticas:
    - Cantidad total de peticiones procesadas.
    - Cantidad de peticiones exitosas y fallidas (según código HTTP).
    - Listar accesos agrupados por dirección IP.
    - Listar recursos más solicitados.
    - Filtrar registros por: errores, IP o recurso).
    - Generar reportes en archivo o pantalla con los informes generados.
