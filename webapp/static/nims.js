  var graph_com;
  var graph_com_dps = [];
  var graph_sa ;
  var graph_sa_dps= [];
  var graph_sv;
  var graph_sv_dps = [];
  var test_img;
  var last_x = 0;



function look_at_test_image(image)
{
  obj = JSON.parse(image)
  var img = document.createElement('img');
  img.src = generateBMPDataURL(obj.intensity);
  img.alt = 'doesnt support data url';
  img.title = ' generate an image ...';

  c = document.getElementById('backscatter');
  ctx = c.getContext('2d');
  ctx.drawImage(img, 0,0, 500,500);

  sa = obj.sa;
  sv = obj.sv;
  com = obj.com;
  graph_sa_dps.push({x: last_x, y: sa});
  if (graph_sa_dps.length > 25) {
      graph_sa_dps.shift()
  }
  
  graph_sv_dps.push({x: last_x, y: sv});
  if (graph_sv_dps.length > 25) {
      graph_sv_dps.shift()
  }
  

  graph_com_dps.push({x: last_x, y: com});

  if (graph_com_dps.length > 25) {
      graph_com_dps.shift()
  }
  last_x += 1;
  
  write_meta_data(obj)
  graph_sa.render()
  graph_sv.render()
  graph_com.render()
}

function write_meta_data(data)
{
  
  return
}
function generate_test_image()
{

    rows = [];

    for (var r = 0; r < 255; r++)
    {
      row = [];
      for (var i = 0; i < 255; i++)
      {
        b = Math.floor(Math.random() * 255);
        px = [0, 0, b];
        row[i] = px;
        //console.log(bmp.pixel[r,i]);
      }
      rows[r] = row;
    }



    var img = document.createElement('img');
    img.src = generateBMPDataURL(rows);
    img.alt = 'doesnt support data url';
    img.title = ' generate an image ...';
    return img;
}



  

        
   


  /*----------------------------------------------------------------------------
  - Name: generate_data()
  - Desc: creates a random sampling of data
  - Input:
  - Output:
  --------------------------------------------------------------------------- */
  function onBodyLoad() {

    graph_sa_dps = []
    graph_sa = new CanvasJS.Chart("graph_sa",
    {
      exportEnabled: true,
      zoomEnabled: true,
      axisX: { interval: 1,
      		   title: "Sample Number",
      		  // labelAngle: 45,
      		 },

      axisY: { interval: 10,
      		   title: "s_a",
      		 },

      title:  { text: "S_A: Random samples, 10hz" },



      data: [{
        type: "line",
        dataPoints: graph_sa_dps
      }]

    });

    graph_sv_dps = []
  	var xinterval = Math.floor(graph_sv_dps.length / 5);
    graph_sv = new CanvasJS.Chart("graph_sv",
    {
      exportEnabled: true,
      zoomEnabled: true,
      axisX: { interval: 1,
      		   title: "Sample Number",
      		  // labelAngle: 45,
      		 },

      axisY: { interval: 10,
      		   title: "s_v",
      		 },

      title:  { text: "S_V: Random Random samples, 10hz" },



      data: [{
        type: "line",
        dataPoints: graph_sv_dps
      }]

    });

  	graph_com_dps = []

    graph_com = new CanvasJS.Chart("graph_com",
    {
      exportEnabled: true,
      zoomEnabled: true,
      axisX: { interval: 1,
      		   title: "Sample Number",
      		  // labelAngle: 45,
      		 },

      axisY: { interval: 10,
      		   title: "Center of Mass",
      		 },

      title:  { text: "Center of Mass: Random Random samples, 10hz"},
      data: [{
        type: "line",
        dataPoints: graph_com_dps
      }]

    });

    graph_com.render();
    graph_sa.render();
    graph_sv.render();

    test_img = generate_test_image();
    c = document.getElementById('backscatter');
    ctx = c.getContext('2d');
    ctx.drawImage(test_img, 0,0, 500, 500);


 
  }


/*----------------------------------------------------------------------------
- Name:
- Desc:
- Input:
- Output:
--------------------------------------------------------------------------- */
function _asLittleEndianHex(value, bytes) {
  // Convert value into little endian hex bytes
  // value - the number as a decimal integer (representing bytes)
  // bytes - the number of bytes that this value takes up in a string

  // Example:
  // _asLittleEndianHex(2835, 4)
  // > '\x13\x0b\x00\x00'

  var result = [];

  for (; bytes>0; bytes--) {
      result.push(String.fromCharCode(value & 255));
      value >>= 8;
  }

  return result.join('');
}

/*----------------------------------------------------------------------------
- Name:
- Desc:
- Input:
- Output:
--------------------------------------------------------------------------- */
function _collapseData(rows, row_padding) {
    // Convert rows of RGB arrays into BMP data
    var i,
        rows_len = rows.length,
        j,
        pixels_len = rows_len ? rows[0].length : 0,
        pixel,
        padding = '',
        result = [];

    for (; row_padding > 0; row_padding--) {
        padding += '\x00';
    }

    for (i=0; i<rows_len; i++) {
        for (j=0; j<pixels_len; j++) {
            pixel = rows[i][j];
            result.push(String.fromCharCode(pixel[2]) +
                        String.fromCharCode(pixel[1]) +
                        String.fromCharCode(pixel[0]));
        }
        result.push(padding);
    }

    return result.join('');
}

/*----------------------------------------------------------------------------
- Name:
- Desc:
- Input:
- Output:
--------------------------------------------------------------------------- */
function generateBMPDataURL(rows) {
      // Expects rows starting in bottom left
      // formatted like this: [[[255, 0, 0], [255, 255, 0], ...], ...]
      // which represents: [[red, yellow, ...], ...]

      if (!window.btoa) {
          alert('Your browser does not support base64 encoding.  Cannot render bitmap.');
          return false;
      }

      var height = rows.length,                                // the number of rows
          width = height ? rows[0].length : 0,                 // the number of columns per row
          row_padding = (4 - (width * 3) % 4) % 4,             // pad each row to a multiple of 4 bytes
          num_data_bytes = (width * 3 + row_padding) * height, // size in bytes of BMP data
          num_file_bytes = 54 + num_data_bytes,                // full header size (offset) + size of data
          file;

      height = _asLittleEndianHex(height, 4);
      width = _asLittleEndianHex(width, 4);
      num_data_bytes = _asLittleEndianHex(num_data_bytes, 4);
      num_file_bytes = _asLittleEndianHex(num_file_bytes, 4);

      // these are the actual bytes of the file...

      file = ('BM' +               // "Magic Number"
              num_file_bytes +     // size of the file (bytes)*
              '\x00\x00' +         // reserved
              '\x00\x00' +         // reserved
              '\x36\x00\x00\x00' + // offset of where BMP data lives (54 bytes)
              '\x28\x00\x00\x00' + // number of remaining bytes in header from here (40 bytes)
              width +              // the width of the bitmap in pixels*
              height +             // the height of the bitmap in pixels*
              '\x01\x00' +         // the number of color planes (1)
              '\x18\x00' +         // 24 bits / pixel
              '\x00\x00\x00\x00' + // No compression (0)
              num_data_bytes +     // size of the BMP data (bytes)*
              '\x13\x0B\x00\x00' + // 2835 pixels/meter - horizontal resolution
              '\x13\x0B\x00\x00' + // 2835 pixels/meter - the vertical resolution
              '\x00\x00\x00\x00' + // Number of colors in the palette (keep 0 for 24-bit)
              '\x00\x00\x00\x00' + // 0 important colors (means all colors are important)
              _collapseData(rows, row_padding)
             );

      return 'data:image/bmp;base64,' + btoa(file);
  }


/*----------------------------------------------------------------------------
- Name:
- Desc:
- Input:
- Output:
--------------------------------------------------------------------------- */
