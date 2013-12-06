$(function() {
    $.widget("metrics.lineplot", {
        options: {
            span: 10 * 1000,
            width: 800,
            height: 100,
        },

        _create: function () {
            this.plot =  $("<div/>").width(this.options.width).height(this.options.height);
            console.log(this.element);
            this.plot.appendTo(this.element);
            this.status = $("<div/>");
            this.status.appendTo(this.element);
            this.data = [];
            this.max = -1;
            this.curr = 0;
        },

        _redraw: function() {
            var fmtVal = function(val) {
                var s = ""; 
                if (val > 1000) { val /= 1000; s = "K"; }
                if (val > 1000) { val /= 1000; s = "M"; }
                if (val > 1000) { val /= 1000; s = "G"; }
                return val.toFixed(1) + s;
            };

            var rdata = [];
            var now = new Date().getTime();
            var newmax = 0;
            for (var i = 1; i < this.data.length; i++) {
                var dv = (this.data[i][1] - this.data[i-1][1]);
                var dt = (this.data[i][0] - this.data[i-1][0]) / 1000;
                var v = dv/dt;
                if (v > newmax) newmax = v;
                rdata.push([this.data[i][0] - now, v]);
            }
            var data2 = [], data3 = [];
            for (var i = 0; i<rdata.length-1; i++) {
                data2[i] = [rdata[i][0], rdata[i][1] * 1.2];
                data3[i] = [rdata[i][0], rdata[i][1] * 0.8];
            }
            //var plotdata = [{data: rdata, lines: {fill: true}}]
            var plotdata = [{data: rdata, lines: {show:true}},
                            {data: data2, id: "top", lines: {show:true, lineWidth: 0}},
                            {data: data3, fillBetween: "top", lines: {show:true, fill: 0.2, lineWidth: 0}}];
            if (newmax > this.max || newmax < this.max / 2) {
                this.max = newmax * 1.4;
                this.plot.plot(plotdata, { xaxis: { min: -this.options.span, max: 0, show: false }, series: {shadowSize: 0}, grid: { borderWidth: 0}, yaxis: { min: 0, max: this.max, tickFormatter: fmtVal}  } );
            } else {
                this.plot.data("plot").setData(plotdata);
                this.plot.data("plot").draw();
            }
            var txt = "-";
            if (rdata.length > 0) {
                var next = rdata[rdata.length - 1][1]
                this.curr = this.curr * 0.8 + next * 0.2;
                txt = fmtVal(this.curr);
            }
            this.status.text(txt);
        },

        _moreData: function(time, val) {
            //console.log(time, val)
            if (this.data.length == 0 || time > this.data[this.data.length - 1][0]) {
                this.data.push([time,val])
                var i = 0;
                while (this.data[i][0] < time - (this.options.span * 1.1)) i++;
                if (i > 0) this.data = this.data.slice(i);
                this._redraw();
            }
        }

    });
});
