var metric_step = 100;

var context = cubism.context().step(100).size(500).clientDelay(0).serverDelay(0);


var undefined_values = [];
for (var i = 0; i < context.size(); i++) {
    undefined_values[i] = NaN;
}

$(function(){
    d3.select("#content").selectAll(".axis")
        .data(["top", "bottom"])
        .enter().append("div")
        .attr("class", function(d) { return d + " axis"; })
        .each(function(d) { d3.select(this).call(context.axis().ticks(12).orient(d)); });

    d3.select("#content")
        .append("div").attr('class','rulebox')
        .append("div").attr("class", "rule")
        .call(context.rule());

});


var last_update;
var metrics_tree;
var metrics_list = [];

$(function(){
    metrics_tree = {
        children: {},
        node: d3.select('#content')
    };
});

function mk_cubism_metric(name, data) {
    return context.metric(function (start, stop, step, callback){
        start = +start; stop = +stop;
        var len = (stop - start) / step;
        var first = data.length - ((last_update - start) / step);
        callback(null, data.slice(first, first + len));
    }, name);
}

$(function(){
    $('#content').on('click', '.name', function(ev) {
        $(this).parent().toggleClass('collapsed');
    });
});

function new_metric(args) {
    var name = args.name;
    var name_parts = name.trim().split('/');
    if (name_parts.length == 0) return;
    var m = metrics_tree;
    for (var i = 0; i < name_parts.length; i++) {
        if (!m.children[name_parts[i]]) {
            var node = m.node.append('div').attr('class','metricset');
            var name = name_parts[i];
            node.append('a').attr('class','name').attr('href','#').text(name);
            m.children[name] = {
                name: name,
                children: {},
                node: node
            };
        } else {
            if (i == name_parts - 1) {
                log("duplicate metric " + name);
                return;
            }
        }
        m = m.children[name_parts[i]];
    }
    $.extend(m, {
        unit: args.unit, 
        type: args.type,
        events: undefined_values.slice(0),
        events_last: null,
        sum: undefined_values.slice(0),
        sum_last: null
    });
    metrics_list.push(m);

    var h = context.horizon();
    m.node
        .selectAll('.horizon')
        .data([mk_cubism_metric(m.name, m.sum)]).enter()
        .append('div').attr('class','horizon').call(h);
}

function update_metric(m, dcycles, events, sum) {
    var fps = 1000 / context.step();
    var hz = dcycles * fps;
    log('hz:'+ hz + ',fps:' + fps);

    if (m.events_last != null) {
        m.events.push(events - m.events_last);
    }
    if (m.sum_last != null) {
        if (m.type == 'state') {
            m.sum.push(sum);
        } else if (m.unit == 'cycles') {
            
            m.sum.push(sum - m.sum_last);
        }
    }
    m.events_last = events;
    m.sum_last = sum;
}

function log(msg) {
    $('#output').append($('<div/>').text(msg));
}

$(function() {
    var url = 'ws://' + window.location.host + '/foo';
    //g = $('#plot').lineplot().data("metrics-lineplot");
    websocket = new WebSocket(url);
    websocket.onopen = function(ev) {
        log('CONNECTED');
    };
    websocket.onclose = function(ev) {
        log('DISCONNECTED');
    };
    websocket.onmessage = function(ev) {
        if (ev.data[0] == 'S' || ev.data[0] == 'A') {
            var args = {}, m;
            var re = /([a-zA-Z_]*)={{(.*?)}}/g;
            while ((m = re.exec(ev.data))) {
                args[m[1]] = m[2];
            }
            if (ev.data[0] == 'A') {
                new_metric(args);
            } else {
                $('#page_title').text('Metrics for pid ' + args.pid);
                console.log(args);
            }
        } else if (ev.data[0] == 'D') {
            var datapoints = ev.data.split(' ');
            sample_cycles = +datapoints[1];
            last_update = +new Date();
            for (var i = 0; i < metrics_list.length; i ++) {
                update_metric(metrics_list[i], sample_cycles, +datapoints[i*2+2], +datapoints[i*2+3]);
            }
        } else {
            log(ev.data);
        }
    }
    websocket.onerror = function(ev) {
        writeToScreen('<span style="color: red; ">ERROR: </span> ' + ev.data);
    };
});
