//============================================================================//
//  Gavin Brock's CSS/JavaScript Animated Odometer
//  Version 1.0 - April 7th 2008
//============================================================================//
//  Copyright (C) 2008 Gavin Brock
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//============================================================================//

function sprintf ( ) {
    // http://kevin.vanzonneveld.net
    // +   original by: Ash Searle (http://hexmen.com/blog/)
    // + namespaced by: Michael White (http://getsprink.com)
    // +    tweaked by: Jack
    // +   improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // +      input by: Paulo Freitas
    // +   improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // +      input by: Brett Zamir (http://brett-zamir.me)
    // +   improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // *     example 1: sprintf("%01.2f", 123.1);
    // *     returns 1: 123.10
    // *     example 2: sprintf("[%10s]", 'monkey');
    // *     returns 2: '[    monkey]'
    // *     example 3: sprintf("[%'#10s]", 'monkey');
    // *     returns 3: '[####monkey]'

    var regex = /%%|%(\d+\$)?([-+\'#0 ]*)(\*\d+\$|\*|\d+)?(\.(\*\d+\$|\*|\d+))?([scboxXuidfegEG])/g;
    var a = arguments, i = 0, format = a[i++];

    // pad()
    var pad = function (str, len, chr, leftJustify) {
        if (!chr) {chr = ' ';}
        var padding = (str.length >= len) ? '' : Array(1 + len - str.length >>> 0).join(chr);
        return leftJustify ? str + padding : padding + str;
    };

    // justify()
    var justify = function (value, prefix, leftJustify, minWidth, zeroPad, customPadChar) {
        var diff = minWidth - value.length;
        if (diff > 0) {
            if (leftJustify || !zeroPad) {
                value = pad(value, minWidth, customPadChar, leftJustify);
            } else {
                value = value.slice(0, prefix.length) + pad('', diff, '0', true) + value.slice(prefix.length);
            }
        }
        return value;
    };

    // formatBaseX()
    var formatBaseX = function (value, base, prefix, leftJustify, minWidth, precision, zeroPad) {
        // Note: casts negative numbers to positive ones
        var number = value >>> 0;
        prefix = prefix && number && {'2': '0b', '8': '0', '16': '0x'}[base] || '';
        value = prefix + pad(number.toString(base), precision || 0, '0', false);
        return justify(value, prefix, leftJustify, minWidth, zeroPad);
    };

    // formatString()
    var formatString = function (value, leftJustify, minWidth, precision, zeroPad, customPadChar) {
        if (precision != null) {
            value = value.slice(0, precision);
        }
        return justify(value, '', leftJustify, minWidth, zeroPad, customPadChar);
    };

    // doFormat()
    var doFormat = function (substring, valueIndex, flags, minWidth, _, precision, type) {
        var number;
        var prefix;
        var method;
        var textTransform;
        var value;

        if (substring == '%%') {return '%';}

        // parse flags
        var leftJustify = false, positivePrefix = '', zeroPad = false, prefixBaseX = false, customPadChar = ' ';
        var flagsl = flags.length;
        for (var j = 0; flags && j < flagsl; j++) {
            switch (flags.charAt(j)) {
                case ' ': positivePrefix = ' '; break;
                case '+': positivePrefix = '+'; break;
                case '-': leftJustify = true; break;
                case "'": customPadChar = flags.charAt(j+1); break;
                case '0': zeroPad = true; break;
                case '#': prefixBaseX = true; break;
            }
        }

        // parameters may be null, undefined, empty-string or real valued
        // we want to ignore null, undefined and empty-string values
        if (!minWidth) {
            minWidth = 0;
        } else if (minWidth == '*') {
            minWidth = +a[i++];
        } else if (minWidth.charAt(0) == '*') {
            minWidth = +a[minWidth.slice(1, -1)];
        } else {
            minWidth = +minWidth;
        }

        // Note: undocumented perl feature:
        if (minWidth < 0) {
            minWidth = -minWidth;
            leftJustify = true;
        }

        if (!isFinite(minWidth)) {
            throw new Error('sprintf: (minimum-)width must be finite');
        }

        if (!precision) {
            precision = 'fFeE'.indexOf(type) > -1 ? 6 : (type == 'd') ? 0 : undefined;
        } else if (precision == '*') {
            precision = +a[i++];
        } else if (precision.charAt(0) == '*') {
            precision = +a[precision.slice(1, -1)];
        } else {
            precision = +precision;
        }

        // grab value using valueIndex if required?
        value = valueIndex ? a[valueIndex.slice(0, -1)] : a[i++];

        switch (type) {
            case 's': return formatString(String(value), leftJustify, minWidth, precision, zeroPad, customPadChar);
            case 'c': return formatString(String.fromCharCode(+value), leftJustify, minWidth, precision, zeroPad);
            case 'b': return formatBaseX(value, 2, prefixBaseX, leftJustify, minWidth, precision, zeroPad);
            case 'o': return formatBaseX(value, 8, prefixBaseX, leftJustify, minWidth, precision, zeroPad);
            case 'x': return formatBaseX(value, 16, prefixBaseX, leftJustify, minWidth, precision, zeroPad);
            case 'X': return formatBaseX(value, 16, prefixBaseX, leftJustify, minWidth, precision, zeroPad).toUpperCase();
            case 'u': return formatBaseX(value, 10, prefixBaseX, leftJustify, minWidth, precision, zeroPad);
            case 'i':
            case 'd':
                number = parseInt(+value, 10);
                prefix = number < 0 ? '-' : positivePrefix;
                value = prefix + pad(String(Math.abs(number)), precision, '0', false);
                return justify(value, prefix, leftJustify, minWidth, zeroPad);
            case 'e':
            case 'E':
            case 'f':
            case 'F':
            case 'g':
            case 'G':
                number = +value;
                prefix = number < 0 ? '-' : positivePrefix;
                method = ['toExponential', 'toFixed', 'toPrecision']['efg'.indexOf(type.toLowerCase())];
                textTransform = ['toString', 'toUpperCase']['eEfFgG'.indexOf(type) % 2];
                value = prefix + Math.abs(number)[method](precision);
                return justify(value, prefix, leftJustify, minWidth, zeroPad)[textTransform]();
            default: return substring;
        }
    };

    return format.replace(regex, doFormat);
}

function Odometer (parentDiv,opts) {
    if (!parentDiv) throw "ERROR: Odometer object must be past a document element.";

    this.digits       = 6;
    this.tenths       = 0;
    this.hundredths   = 0;
    this.digitHeight  = 40;
    this.digitPadding = 0;
    this.digitWidth   = 30;
    this.bustedness   = 2;
    this.fontStyle    = "font-family: Courier New, Courier, monospace; font-weight: 900;";
    this.value        = -1;

    for (var key in opts) { this[key] = opts[key]; }

    var style = {
        digits:        "position:absolute; height:"+this.digitHeight+"px; width:"+(this.digitWidth-(2*this.digitPadding))+"px; "+
                       "padding:"+this.digitPadding+"px; font-size:"+(this.digitHeight-(2*this.digitPadding))+"px; "+
                       "background:black; color:white; text-align:center; "+this.fontStyle,
        decimal:       "position:absolute; height:"+this.digitHeight+"px; width:"+((this.digitWidth/4)-(2*this.digitPadding))+"px; "+
                       "padding:"+this.digitPadding+"px; font-size:"+(this.digitHeight-(2*this.digitPadding))+"px; "+
                       "background:black; color:white; text-align:center; "+"font-family: Helvetica; font-weight: 900;",
        columns:       "position:relative; float:left; overflow:hidden;"+
                       "height:"+this.digitHeight+"px; width:"+this.digitWidth+"px;",
        columns_dec:   "position:relative; float:left; overflow:hidden;"+
                       "height:"+this.digitHeight+"px; width:"+this.digitWidth/4+"px;",
        highlight:     "position:absolute; background:white; opacity:0.25; filter:alpha(opacity=25); width:100%; left:0px;",
        lowlight:      "position:absolute; background:black; opacity:0.25; filter:alpha(opacity=25); width:100%; left:0px;",
        sidehighlight: "position:absolute; background:white; opacity:0.50; filter:alpha(opacity=50); height:100%; top:0px;",
        sidelowlight:  "position:absolute; background:black; opacity:0.50; filter:alpha(opacity=50); height:100%; top:0px;"
    };

    var highlights = [
        "top:20%;   height:32%;" + style.highlight,
        "top:27.5%; height:16%;" + style.highlight,
        "top:32.5%; height:6%;"  + style.highlight,
        "right:0%;  width:6%;"   + style.sidelowlight,
        "left:0%;   width:4%;"   + style.sidehighlight,
        "top:0%;    height:14%;" + style.lowlight,
        "bottom:0%; height:25%;" + style.lowlight,
        "bottom:0%; height:8%;"  + style.lowlight
    ];

    this.setDigitValue = function (digit, val, frac) {
	var di = digitInfo[digit];
       	var px = Math.floor(this.digitHeight * frac);
	px = px + di.offset;
	if (val != di.last_val) {
		var tmp = di.digitA;
		di.digitA = di.digitB;
		di.digitB = tmp;
        	di.digitA.innerHTML = val;
        	di.digitB.innerHTML = (1+Number(val)) % 10;
		di.last_val = val;
	}
	if (px != di.last_px) {
        	di.digitA.style.top = (0-px)+"px";
        	di.digitB.style.top = (0-px+this.digitHeight)+"px";
		di.last_px = px;
	}
    };


    this.set = function (inVal) {
		if (inVal < 0) throw "ERROR: Odometer value cannot be negative.";
		this.value = inVal;
		
		var numb;
		var frac;
		if (this.tenths) {
			numb = Math.floor(inVal * 10);
			frac = inVal*10 - numb;
			numb = sprintf("%0"+String(this.digits-1)+".1f", numb/10);
		} else if (this.hundredths) {
			numb = Math.floor(inVal * 100);
			frac = inVal*100 - numb;
			numb = sprintf("%0"+String(this.digits-1)+".2f", numb/100);
		} else {
			numb = Math.floor(inVal);
			frac = inVal - numb;
			numb = sprintf("%0"+String(this.digits-1)+".0f", numb);
		}

		for (var i=0; i < this.digits; i++) {
			var num = numb.substring(numb.length-i-1, numb.length-i) || 0;
			this.setDigitValue(this.digits-i-1, num, frac);
			if (num != 9) frac = 0;
		}
	};

    this.get = function () {
        return(this.value);
    };


    var odometerDiv = document.createElement("div")
    odometerDiv.setAttribute("id","odometer");
    odometerDiv.style.cssText="text-align: left";
    parentDiv.appendChild(odometerDiv);

    var digitInfo = new Array();
    for (var i=0; i < this.digits; i++) {
        var digitDivA = document.createElement("div");
        digitDivA.setAttribute("id","odometer_digit_"+i+"a");

        var digitDivB = document.createElement("div");
        digitDivB.setAttribute("id","odometer_digit_"+i+"b");

        var digitColDiv = document.createElement("div");

        if ( (this.tenths && i == this.digits - 2) || (this.hundredths && i == this.digits - 3) ) {
			digitDivB.style.cssText=style.decimal;
			digitDivA.style.cssText=style.decimal;
			digitColDiv.style.cssText = style.columns_dec;
        } else {
        	digitDivA.style.cssText=style.digits;
        	digitDivB.style.cssText=style.digits;
			digitColDiv.style.cssText = style.columns;
        }


        digitColDiv.appendChild(digitDivB);
        digitColDiv.appendChild(digitDivA);

        for (var j in highlights) {
            var hdiv = document.createElement("div");
            hdiv.innerHTML="<p></p>"; // For Dumb IE
            hdiv.style.cssText = highlights[j];
            digitColDiv.appendChild(hdiv);
        }
        odometerDiv.appendChild(digitColDiv);
	var offset = Math.floor(Math.random()*this.bustedness);
	digitInfo.push({digitA:digitDivA, digitB:digitDivB, last_val:-1, last_px: -1, offset:offset});
    };


    if (this.tenths) {
	digitInfo[this.digits - 1].digitA.style.background = "#cccccc";
	digitInfo[this.digits - 1].digitB.style.background = "#cccccc";
	digitInfo[this.digits - 1].digitA.style.color = "#000000";
	digitInfo[this.digits - 1].digitB.style.color = "#000000";
    }

    if (this.hundredths) {
	digitInfo[this.digits - 1].digitA.style.background = "#cccccc";
	digitInfo[this.digits - 1].digitB.style.background = "#cccccc";
	digitInfo[this.digits - 1].digitA.style.color = "#000000";
	digitInfo[this.digits - 1].digitB.style.color = "#000000";
	digitInfo[this.digits - 2].digitA.style.background = "#cccccc";
	digitInfo[this.digits - 2].digitB.style.background = "#cccccc";
	digitInfo[this.digits - 2].digitA.style.color = "#000000";
	digitInfo[this.digits - 2].digitB.style.color = "#000000";
    }


    if (this.value >= 0) this.set(this.value);
}
