var graphs;
var color_map;
var initial_connection = true;
var last_ping = 0;
var paths = [];

var app_config;
var polar_map = undefined;

function upper_bound(arr, val) {
    if (arr.length == undefined)
        return undefined;
    for (var i = 0; i < arr.length; i++)
        if (arr[i] > val)
            return i;
    return undefined;
}

function ping_to_polar_map(ping) {  // generate polar map
    write_log('Generating polar transform.');
    var min_range = ping.range_min;
    var max_range = ping.range_max;
    var num_samples = ping.num_samples;
    var num_beams = ping.num_beams;
    var angles_deg = ping.beam_angles;

    var span = max_range - min_range;
    var meters_per_sample = span / num_samples;
    var samples_per_meter = num_samples / span;

    var rows = Math.floor(max_range * samples_per_meter); // to offset from ducer, add extra length
    var edge_rad = (90 - (ping.sector_size / 2)) * (Math.PI / 180.0);
    var cols = Math.floor(max_range * Math.cos(edge_rad) * 2 * samples_per_meter);

    polar_map = new Array(rows).fill(new Array(cols));

    for (var c = 0; c < num_samples; c++) {
        var range = min_range + (c * meters_per_sample);
        for (var r = 0; r < num_beams; r++) {
            var theta = angles_deg[r] * (Math.PI / 180.0);
            var x = range * Math.cos(theta);
            var y = range * Math.sin(theta);
            polar_map[c][r] = [x, y];
        }
    }

    return polar_map;
}

/* ----------------------------------------------------------------------------
 - Name:
 - Desc:
 - Input:
 - Output:
 ---------------------------------------------------------------------------- */
function handle_web_socket() {
    console.log("handle_web_socket");

    if (initial_connection == true) {
        write_log("Established initial connection to NIMS.");
        console.log(" - initial connection");
        initial_connection = false;
        color_map = generate_heat_map();
        graphs =
        {
            Sv_area: new graph_object("Area Backscattering Strength (Sa)", 1,"threshold_avgsv"  ),
            Sv_volume: new graph_object("Vol. Backscattering Strength (Sv)", 1, "threshold_sv"),
            center_of_mass: new graph_object("Center of Mass", 1,"threshold_center_of_mass" ),
            inertia: new graph_object("Inertia", 25, "threshold_inertia" ),
            proportion_occupied: new graph_object("Proportion Occupied", .1, "threshold_prop_occ"),
            equivalent_area: new graph_object("Equivalent Area", 1, "threshold_equiv_area"),
            aggregate_index: new graph_object("Aggregation Index", .1, "threshold_avgsv")
        };

        image = render_static_image();
        //display_image(image, "BSCAN");

        var device = "Unknown Transducer Model";
        var version = "xx";
        var pingid = "xxxx";
        var soundspeed = "xxxx";
        var num_samples = "xxxx";
        var range_min = "xx";
        var range_max = "xx";
        var num_beams = "xxx";
        var freq = "xxxxxxxx";
        var pulse_len = "xx";
        var pulse_rep = "xx";
        var ts = "xxxxxxxxx";

        var elem = document.getElementById("meta");
        var inner = "<fieldset><legend>Sonar Settings</legend>";
        inner += "<span><pre>" + device + ", version " + version + "</span>\n";
        inner += "<span><pre>     Ping ID: " + pingid + "\t\t";
        inner += "   Ping Time: " + ts + "</span>\n";
        inner += "<span><pre>        Freq: " + freq + "\t\t";
        inner += " Sound Speed: " + soundspeed + "</span>\n";
        inner += "<span><pre>   Range Min: " + range_min + "\t\t";
        inner += "   Range Max: " + range_max + "</span>\n";
        inner += "<span><pre>   Num Beams: " + num_beams + "\t\t";
        inner += " Num Samples: " + num_samples + "</span>\n";
        inner += "<span><pre>   Pulse Len: " + pulse_len + "\t\t";
        inner += "   Pulse Rep: " + pulse_rep + "</span>\n";
        inner += "</fieldset>";
        elem.innerHTML = inner;

        return;
    }
    write_log("Reconnected.");
    console.log(" - handling possible reconnection.");
}


/* ----------------------------------------------------------------------------
 - Name: graph_object()
 - Input: str: graph title
 int: num values on x
 int: interval to grpah on y (10)
 - Output: a graph object
 - Desc: Generates a graph for ChartJS with some default values
 ---------------------------------------------------------------------------- */
function graph_object(graph_name, y_interval, threshold_key) {
    y_interval = typeof y_interval != 'undefined' ? y_interval : 10;
    console.log(graph_name + ":" + y_interval);
    this.name = graph_name;
    this.points = [];
    this.threshold = [];
    this.threshold_key = threshold_key
    this.graph = new CanvasJS.Chart(graph_name,
        {
            exportEnabled: true,
            zoomEnabled: true,
            axisX: {
                interval: 25,
                title: "Ping",
                labelAngle: 45,
            },

            axisY: {
                //interval: y_interval,
                title: graph_name
            },

            title: {text: graph_name},

            data: [
            {
                type: "line",
                dataPoints: this.points
            },
            {
                type: "line",
                dataPoints: this.threshold
            }
            ]

        });
    this.graph.render();
    return this;

}

graph_object.prototype.append = function (x_val, y_val, num_values) {
    num_values = typeof num_values !== 'undefined' ? num_values : 50;
    y_thresh = app_config['ECHOMETRICS'][this.threshold_key]
    y_thresh = parseInt(y_thresh)
    this.points.push({x: x_val, y: y_val});
    if (!isNaN(y_thresh))
        this.threshold.push({x: x_val, y: y_thresh});
    if (this.points.length > num_values) {
        this.points.shift();
        this.threshold.shift();
    }
    this.graph.render();
};

graph_object.prototype.reset = function () {
    while (this.points.length > 0) {
        this.points.shift();
        this.threshold.shift();
    }
        this.graph.render();
};
/* ----------------------------------------------------------------------------
 - Name: generate_heat_map()
 - Desc: creates a red scale rgb array from 0 to 256; maps intensity to rgb
 - Input:
 - Output: Array of rgb arrays
 ----------------------------------------------------------------------------- */
function generate_heat_map() {
    var cmap = [];
    for (var i = 0; i < 256; i++) {
        var color = [i, Math.floor(.4 * i), Math.floor(.2 * i)];
        for (var j = 0; j < 4; j++) {
            cmap.push(color)
        }
    }
    return cmap;
}

function exportConfig() {
    write_log("Wrote new configuration.");
    // read every row in the table, edit the config, stringify it, send it
    var tracker_table = document.getElementById('tracker_table');
    var rowNum = 2;
    for (var i = rowNum; i < tracker_table.rows.length; i++) {
        // cell0+1 = tracker data, cell3+4 = echometrics
        row = tracker_table.rows[i];
        key = row.cells[0].innerText.trim()
        if (key.length > 0) {
            app_config['TRACKER'][key] = row.cells[1].innerText.trim();
            write_log('writing key: ' + key)
        }
        key = row.cells[2].innerText.trim()
        if (key.length > 0) {
            write_log('writing key: ' + key);
            app_config['ECHOMETRICS'][key] = row.cells[3].innerText.trim();
        }
    }


    config = JSON.stringify(app_config);
    ws.send(config)

}

function write_log(message) {
    var elem = document.getElementById("log");
    log.innerHTML = "<pre>     " + message + " :: " + Date() + "</pre>";
}
function process_config(data) {

    write_log("Received new configuration data.");

    app_config = JSON.parse(data);
    console.log("app_config:" + app_config);

    var track_config = app_config['TRACKER'];
    var application = app_config['APPLICATIONS'];
    var metrics = app_config['ECHOMETRICS']
    // remove all existing rows and populate with new data
    var tracker_table = document.getElementById('tracker_table');
    var rowCount = tracker_table.rows.length;
    while (--rowCount > 1)
        tracker_table.deleteRow(rowCount);

    length = track_config.length;
    for (var key in track_config) {
        if (key == "")
            continue;
        if (track_config.hasOwnProperty(key)) {
            var row = tracker_table.insertRow();
            var cell1 = row.insertCell(0);
            cell1.bgColor = "CadetBlue";
            cell1.align = "right";
            cell1.innerHTML = "<pre>" + key;
            var cell2 = row.insertCell(1);
            cell2.align = "left";
            cell2.innerHTML = "<pre>" + String(track_config[key]);
            cell2.contentEditable = "true"

        }
    }


    var rowNum = 2; // first td
    for (var key in metrics) {
        if (key == "queue_name")
            continue
        if (rowNum >= tracker_table.rows.length) {
            row = tracker_table.insertRow();
            var cell = row.insertCell(0);
            cell.bgColor = "CadetBlue";
            cell = row.insertCell(1);
            cell.bgColor = "CadetBlue"
        }
        else {
            var row = tracker_table.rows[rowNum];
        }

        var cell3 = row.insertCell(2);
        cell3.bgColor = "CadetBlue";
        cell3.innerHTML = "<pre>" + key;
        cell3.align = "right";
        var cell4 = row.insertCell(3);
        //cell4.bgColor = "LightSeaGreen";
        cell4.innerHTML = "<pre>" + metrics[key];
        cell4.align = "left";
        cell4.contentEditable = "true"
        rowNum++;

    }
}

function process_input(jsonobj) {
    var data = JSON.parse(jsonobj);
    if (data.type == 'ping')
        process_ping(data);
    else if (data.type == 'metrics')
        process_metrics(data);
    else if (data.type == 'tracks')
        process_tracks(data);
    else if (data.type == 'config')
        app_config = data
    else
        write_log('Recieved unknow data type')
}

/* ----------------------------------------------------------------------------
 - Name: process_ping()
 - Desc: processes JSON ping data and updates the screen
 - Input: data: JSON ping data
 - Output: none
 ----------------------------------------------------------------------------- */
function process_ping(ping_data) {
    write_meta_data(ping_data);
    if (polar_map == undefined)
        polar_map = ping_to_polar_map(ping_data);

    //var image = render_ppi_image(obj);
    var image = render_bscan_image(ping_data);
    display_image(image, "BSCAN", ping_data);

}

function process_metrics(metrics) {
    if (metrics.pingid < last_ping) {
        graphs.Sv_area.reset();
        graphs.Sv_volume.reset();
        graphs.center_of_mass.reset();
        graphs.inertia.reset();
        graphs.equivalent_area.reset();
        graphs.aggregate_index.reset();
        graphs.proportion_occupied.reset();

    } // handles test data looping and temporarily borking up the graphs

    last_ping = metrics.pingid;
    graphs.Sv_area.append(metrics.pingid, metrics.sv_area);
    graphs.Sv_volume.append(metrics.pingid, metrics.sv_volume);
    graphs.center_of_mass.append(metrics.pingid, metrics.center_of_mass);
    graphs.inertia.append(metrics.pingid, metrics.inertia);
    graphs.equivalent_area.append(metrics.pingid, metrics.equivalent_area);
    graphs.aggregate_index.append(metrics.pingid, metrics.aggregation_index);
    graphs.proportion_occupied.append(metrics.pingid, metrics.proportion_occupied);
}

function process_tracks(track_data) {
    tracks = track_data.tracks;
    write_log('got track data')
    if (track_data.num_tracks == 0)
        return;
    write_log('had tracks')
    for (var i = 0; i < tracks.length; i++) {
        var track = tracks[i];
        var track_id = track[0];
        for (var j = 0; j < paths.length; j++) {
            paths[j].age++;
            if (paths[j].age > 10) {
                paths[j].age = 10;
                paths[j].points.shift();
            }

            path = paths[j];
            var found = false;
            if (path.id == track_id) {
                var point = [track[1], track[2]];
                path.points.push(point);
                found = true;
            }
            if (found == false) {
                var new_path = {id: track_id, age: 1, points: [[track[1], track[2]]]};
                paths.push(path);
            }
        }
    }
}

/* ----------------------------------------------------------------------------
 - Name: display_image(image, image_type)
 - Desc: takes an image an lays it out on the website
 - Input: Image array (rgb values)
 str: image_type (PPI, BSCAN, WATER_COLUMN)
 - Output: None
 ----------------------------------------------------------------------------- */
function display_image(image, image_type, obj) {
    // ignoring image type for now
    var img = document.createElement('img');

    img.src = generateBMPDataURL(image);
    img.alt = "Your browser doesn't support data URLs.";
    img.title = "PPI Display";

    c = document.getElementById('backscatter');
    ctx = c.getContext('2d');
    ctx.drawImage(img, 0, 0, 500, 500);

    if (image_type == "BSCAN") {

        // draw axis layers here, but we need the sonar data ...
        // i.e. range and num samples.
        ctx.strokeStyle = "#CCCCCC";
        ctx.fillStyle = "#CCCCCC";
        console.log("Drawing AXIS Layer for BSCAN");
        var min_range = obj.range_min;
        var max_range = obj.range_max;
        var num_samples = obj.num_samples;
        var num_beams = obj.num_beams;

        var range = max_range - min_range;
        var samps_per_meter = num_samples / range;
        // decide how many lines to draw - lets say 5 since the m3 ppi does this
        var interval = num_samples / 5;

        var axis = new Path2D();
        for (var i = 0; i < 5; i++) {
            axis.moveTo(0, i * 100 + 1);
            axis.lineTo(500, i * 100 + 1);

            meters = max_range - (i * (max_range / 5));
            label = meters + " m";
            ctx.fillText(label, 5, (i * 100) + 12);
            ctx.fillText(label, 475, (i * 100) + 12);
        }
        ctx.stroke(axis);

        sector_size = obj.sector_size;
        min_angle = obj.min_angle;
        max_angle = obj.max_angle;

        beam_width = sector_size / num_beams;


        var axis = new Path2D();
        var interval = 500 / 7;
        var beam_interval = sector_size / 7;

        for (var i = 1; i < 7; i++) {
            axis.moveTo(i * interval, 500);
            axis.lineTo(i * interval, 490);

            angle = min_angle + (beam_interval * i);
            angle = angle.toPrecision(3);
            label = angle + ' d';
            ctx.fillText(label, i * interval - 10, 487);

        }
        label = max_angle + " deg";
        //ctx.FillText(label, i * interval - 40, 487);

        ctx.stroke(axis);

        //return;
        ctx = c.getContext('2d');
        var tracks = obj.tracks;
        if (tracks == undefined) return;


        beam_width = sector_size / num_beams; // this is actually variable ;(
        beams = [];
        for (var i = 0; i < num_beams; i++) {
            beams.push(min_angle + (i * beam_width))
        } // later we'll do a beams reduce.

        for (var i = 0; i < tracks.length; i++) {
            track = tracks[i];
            console.log("track" + track);
            width = track["width"];
            height = track["height"];

            px = Math.abs(track["min_bearing_deg"] - track['max_bearing_deg']) / 2;
            px = track['min_bearing_deg'] + px;
            console.log('bearing = ' + px);
            px = Math.abs(px - min_angle) / sector_size;
            console.log(' % = ' + px);

            px = px * 500;

            py = (track["min_range_m"] + (.5 * height)) / max_range;
            py = 500 - (py * 500);


            //  ctx.save();
            //  ctx.scale(1, height / width);
            ctx.beginPath();
            ctx.moveTo(px, py - height / 2);
            ctx.bezierCurveTo(px + width / 2, py - height / 2,
                px + width / 2, py + height / 2,
                px, py + height / 2);

            ctx.bezierCurveTo(px - width / 2, py + height / 2,
                px - width / 2, py - height / 2,
                px, py - height / 2);

            xangle = track['min_bearing_deg'] + Math.abs(track["min_bearing_deg"] - track['max_bearing_deg']) / 2;
            xangle = xangle.toPrecision(3);
            yrange = (track["min_range_m"] + track["max_range_m"]) / 2;
            yrange = range.toPrecision(3);
            console.log('range = ' + track['min_range_m']);
            label = track['id'];
            ctx.fillText(label, px, py - 2);
            //ctx.arc(px, py, width, 0, 2 * Math.PI, false);
            //  ctx.restore()


            ctx.strokeStyle = '#ffffff';
            ctx.stroke();
            ctx.globalAlpha = 1;
            ctx.closePath();

            //}
        }
        //ctx.arc(250, 250, 20, 0, 2 * Math.PI, false);
        //ctx.fillStyle = 'green';

        //ctx.lineWidth = 2;


    }

}

/* ----------------------------------------------------------------------------
 - Name:  write_meta_data
 - Desc: JSON ping meta data is written to screen
 - Input: JSON Ping meta data
 - Output:
 ----------------------------------------------------------------------------- */
function write_meta_data(data) {
    var device = data.device;
    var version = data.version;
    var pingid = data.pingid;
    var ping_sec = data.ping_sec;
    var ping_ms = data.ping_ms;
    var soundspeed = data.soundspeed;
    var num_samples = data.num_samples;
    var range_min = data.range_min;
    var range_max = data.range_max;
    var num_beams = data.num_beams;
    var freq = data.freq;
    var pulse_len = data.pulse_len;
    var pulse_rep = data.pulse_rep;

    var elem = document.getElementById("meta");
    var inner = "<fieldset><legend>Sonar Settings</legend>";
    inner += "<span><pre>" + device + ", version " + version + "</span>\n";
    inner += "<span><pre>         Ping ID: " + pingid + "\t\t";
    inner += "  Ping Time (HMS): " + data.ts + "</span>\n";
    inner += "<span><pre>       Freq (hz): " + freq + "\t";
    inner += "Sound Speed (m/s): " + soundspeed + "</span>\n";
    inner += "<span><pre>   Range Min (m): " + range_min + "\t\t";
    inner += "    Range Max (m): " + range_max + "</span>\n";
    inner += "<span><pre>       Num Beams: " + num_beams + "\t\t";
    inner += "      Num Samples: " + num_samples + "</span>\n";
    inner += "<span><pre>  Pulse Len (ms): " + pulse_len + "\t\t";
    inner += "   Pulse Rep (hz): " + pulse_rep + "</span>\n";
    inner += "</fieldset>";

    console.log("inner = " + inner);
    elem.innerHTML = inner;

}

function render_bscan_image(ping_data) {

    data = ping_data.intensity;
    numS = ping_data.num_samples;
    numB = ping_data.num_beams;


    cols = [];
    for (var s = 0, k=0; s < numS; s+=3, k+=1) { // s cols, b rows
        row = [];
        for (var b = 0; b < numB; b++) {
            var bb = (numB - 1) - b; // lr flip
            var target = data[bb + (s * numB)];
            var key = Math.floor(target * 500); // magic number
            if (key < 0)
                key = 0;
            if (key > 1000)
                key = 1000;

            row[b] = color_map[key];
        }
        cols[k] = row

    }
    return cols;
}
/* ----------------------------------------------------------------------------
 - Name: create_ppi_image
 - Desc: Take 2d ping intensity data and converts it to an rgb 2d array
 - Input: ping data parsed from JSON
 - Output: Array[Num Samples][Num Beams] in RGB
 ---------------------------------------------------------------------------- */
function render_ppi_image(ping_data) {

    var deg_to_rad = 0.0174532925;

    data = ping_data.intensity;
    numX = ping_data.num_beams;
    numY = ping_data.num_samples;
    min_angle = ping_data.min_angle;
    max_angle = ping_data.max_angle;
    sector_size = ping_data.sector_size;


    angle_offset = (180 - sector_size) / 2;
    beam_width = sector_size / numX;

    max_reach = numY * Math.cos(deg_to_rad * angle_offset);
    max_reach = Math.floor(max_reach);

    rows = new Array(numY + 1);


    rows = [];
    for (var r = 0; r < numY; r++) {
        row = [];
        for (var i = 0; i < max_reach; i++) {
            px = [0, 0, 0];
            row[i] = px;
            //console.log(bmp.pixel[r,i]);
        }
        rows[r] = row;
    }


    var beam_angles = new Array(numX);
    for (var i = 0; i < numX; i++)
        beam_angles[i] = 180 - angle_offset - (i * beam_width);

    for (var y = 0; y < numY; y++) {
        for (var x = 0; x < numX; x++) {
            var angle = beam_angles[x];
            var center_angle = beam_angles[x];
            var left_angle = center_angle - beam_width;
            var right_angle = center_angle + beam_width;
            var angle_step = beam_width / 5.0;

            for (var angle = left_angle; angle < right_angle; angle += angle_step) {
                if (angle > 180)
                    continue;
                if (angle < 0)
                    continue;
                var xx = (numX - 1) - x;
                var target = data[y + (xx * numY)];
                var key = Math.floor(target); // magic number
                if (key < 0)
                    key = 0;
                if (key > 1000)
                    key = 1000;
                beam_angle = angle; //beam_angles[x];
                xcoord = y * Math.cos(deg_to_rad * beam_angle) + numY / 2;
                ycoord = y * Math.sin(deg_to_rad * beam_angle);

                if (typeof color_map[key] == 'undefined') {
                    console.log("undefined cmap: key = ", +key)
                }
                rows[Math.floor(ycoord)][Math.floor(xcoord)] = color_map[key];
            }
        }
    }

    return rows;

}


/* ----------------------------------------------------------------------------
 - Name: render_static_image()
 - Desc: generates a static image for the initial display
 - Input: None
 - Output: Array: [rows[row]] in (r, g, b)
 ---------------------------------------------------------------------------- */
function render_static_image() {
    rows = [];
    for (var r = 0; r < 255; r++) {
        row = [];
        for (var i = 0; i < 255; i++) {
            b = Math.floor(Math.random() * 128);
            //px = [b, .5 * b, 0];
            px = [0, 0, 0];
            row[i] = px;
            //console.log(bmp.pixel[r,i]);
        }
        rows[r] = row;
    }

    return rows;
}

/*----------------------------------------------------------------------------
 - Name: generate_data()
 - Desc: creates a random sampling of data
 - Input:
 - Output:
 --------------------------------------------------------------------------- */
function onBodyLoad() {

    var device = "";
    var version = "";
    var pingid = "";
    var soundspeed = "";
    var num_samples = "";
    var range_min = "";
    var range_max = "";
    var num_beams = "";
    var freq = "";
    var pulse_len = "";
    var pulse_rep = "";
    var ts = "";

    var elem = document.getElementById("meta");
    var inner = "";
    inner += "<span><pre>" + device + ", version " + version + "</span>\n";
    inner += "<span><pre>     Ping ID: " + pingid + "\t\t";
    inner += "   Ping Time: " + ts + "</span>\n";
    inner += "<span><pre>        Freq: " + freq + "\t\t";
    inner += " Sound Speed: " + soundspeed + "</span>\n";
    inner += "<span><pre>   Range Min: " + range_min + "\t\t";
    inner += "   Range Max: " + range_max + "</span>\n";
    inner += "<span><pre>   Num Beams: " + num_beams + "\t\t";
    inner += " Num Samples: " + num_samples + "</span>\n";
    inner += "<span><pre>   Pulse Len: " + pulse_len + "\t\t";
    inner += "   Pulse Rep: " + pulse_rep + "</span>\n";

    elem.innerHTML = inner;


    test_img = generate_test_image();
    c = document.getElementById('backscatter');
    ctx = c.getContext('2d');
    ctx.drawImage(test_img, 0, 0, 500, 500);


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

    for (; bytes > 0; bytes--) {
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

    for (i = 0; i < rows_len; i++) {
        for (j = 0; j < pixels_len; j++) {
            pixel = rows[i][j];
            try {
                result.push(String.fromCharCode(pixel[2]) +
                    String.fromCharCode(pixel[1]) +
                    String.fromCharCode(pixel[0]));
            }
            catch (err) {
                console.log("ERROR: " + err.message);
                  console.log("Pixel at i,j:" + i + "," + j);
                  console.log(pixel);

            }
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

    console.log("Rendering Image");
    var height = rows.length,                                // the number of rows
        width = height ? rows[0].length : 0,                 // the number of columns per row
        row_padding = (4 - (width * 3) % 4) % 4,             // pad each row to a multiple of 4 bytes
        num_data_bytes = (width * 3 + row_padding) * height, // size in bytes of BMP data
        num_file_bytes = 54 + num_data_bytes,                // full header size (offset) + size of data
        file;
    console.log(" - " + height + "x" + width + " px");
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
var $TABLE = $('#table');
var $BTN = $('#export-btn');
var $EXPORT = $('#export');

$('.table-add').click(function () {
    var $clone = $TABLE.find('tr.hide').clone(true).removeClass('hide table-line');
    $TABLE.find('table').append($clone);
});

$('.table-remove').click(function () {
    $(this).parents('tr').detach();
});

$('.table-up').click(function () {
    var $row = $(this).parents('tr');
    if ($row.index() === 1) return; // Don't go above the header
    $row.prev().before($row.get(0));
});

$('.table-down').click(function () {
    var $row = $(this).parents('tr');
    $row.next().after($row.get(0));
});

// A few jQuery helpers for exporting only
jQuery.fn.pop = [].pop;
jQuery.fn.shift = [].shift;

$BTN.click(function () {
    var $rows = $TABLE.find('tr:not(:hidden)');
    var headers = [];
    var data = [];

    // Get the headers (add special header logic here)
    $($rows.shift()).find('th:not(:empty)').each(function () {
        headers.push($(this).text().toLowerCase());
    });

    // Turn all existing rows into a loopable array
    $rows.each(function () {
        var $td = $(this).find('td');
        var h = {};

        // Use the headers from earlier to name our hash keys
        headers.forEach(function (header, i) {
            h[header] = $td.eq(i).text();
        });

        data.push(h);
    });

    // Output the result
    $EXPORT.text(JSON.stringify(data));
});
