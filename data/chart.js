      
      // Webpage Javascript to chart multiple ThingSpeak channels on two axis with navigator, load historical data, and export cvs data.
      // Public Domain, by turgo.
      //  The charting library is called HighStock.  It is awesome!  HighSoft, the owners say, 
      //   "Do you want to use Highstock for a personal or non-profit project? Then you can use Highchcarts for 
      //   free under the  Creative Commons Attribution-NonCommercial 3.0 License. "
var dynamicChart;
var channelsLoaded = 0;
// put your ThingSpeak Channel#, Channel Name, and API keys here.
// fieldList shows which field you want to load, and which axis to display that field on, 
// the 'T' Temperature left axis, or the 'O' Other right axis.
var channelKeys =[];
/*
function calcDp(temp, rh) {
    //Convert to C
    temp = Number(parseFloat((temp-32.0000000000000001)/1.8)).toFixed(10);
    var dew_numer = 243.04*(Math.log(rh/100.0)+((17.625*eval(temp))/(eval(temp)+243.04)));   //Math.log(rh/100*6.112/6.1078*Math.exp((17.67*temp)/(temp-0+243.5)));
    var dew_denom = 17.625-Math.log(rh/100.0)-((17.625*eval(temp))/(eval(temp)+243.04));
    var dummy_dew = dew_numer/dew_denom;
    dummy_dew = (dummy_dew * 1.8) + 32;
    return dummy_dew;
}
*/
function calcDp(T,RH) {
    T = Number(parseFloat((T-32.0000000000000001)/1.8)).toFixed(10)
    var tA = Math.pow(RH/100,1/8);
    return (((112 + (0.9 * T))) * tA + (0.1 * T) - 112) * 1.8 + 32;
    
}

/*
function calcDp(temp, rh) {
   var tem = -1.0*temp;
   var eln = Math.log((rh/100.0*(6.112*Math.exp(-1.0*17.67*tem/(243.5 - tem))))/6.112);
   return  -243.5*eln/(eln - 17.67 );
}
*/
channelKeys.push({channelNumber:92880, name:'Pool',key:'BLY3AV0VL7SW1IUO',
                  fieldList:[{field:2,axis:'T'}]});
channelKeys.push({channelNumber:92874, name:'Home',key:'IKHYSGFOBDVNAOUU',
   addField:{axis:'T', chFunc: calcDp},
   fieldList:[{field:2,axis:'T'},{field:3,axis:'O'}]});
    
// user's timezone offset
var myOffset = new Date().getTimezoneOffset();

// converts date format from JSON
function loadWeather() {
  var weatherDisplay = document.getElementById('showWeather2');
  fetch('http://api.openweathermap.org/data/2.5/weather?APPID=af8a9da471a8a060903c91035517e91f&zip=85339,us&units=imperial&type=accurate')
    .then((res)=> {
        return res.json();
        })
  .then((json)=> {
      weatherDisplay.style.visibility = 'visible';
      weatherDisplay.innerHTML = '<div><span class="label">Temperature</span> <span class="answer">' + json.main.temp +'F</span></div>';
      weatherDisplay.innerHTML += '<div><span class="label">Humidity</span> <span class="answer">' + json.main.humidity + '%</span></div>'; 
      weatherDisplay.innerHTML += '<div><span class="label">DewPoint</span> <span class="answer">' + calcDp(parseFloat(json.main.temp),parseFloat(json.main.humidity)) + '%</span></div>'; 
      weatherDisplay.innerHTML += '<div><span class="label">Cloudiness</span> <span class="answer">' + json.clouds.all + '%</span></div>'; 
      })
}

function getChartDate(d) {
    // get the data using javascript's date object (year, month, day, hour, minute, second)
    // months in javascript start at 0, so remember to subtract 1 when specifying the month
    // offset in minutes is converted to milliseconds and subtracted so that chart's x-axis is correct
    return Date.UTC(d.substring(0,4), d.substring(5,7)-1, d.substring(8,10), d.substring(11,13), d.substring(14,16), d.substring(17,19)) - (myOffset * 60000);
}

      // Hide all series, via 'Hide All' button.  Then user can click on serries name in legent to show series of interest.      
function HideAll(){
  for (var index=0; index<dynamicChart.series.length; index++)  // iterate through each series
  { 
    if (dynamicChart.series[index].name == 'Navigator')
      continue;
    dynamicChart.series[index].hide();
    //window.console && console.log('Series Number:',index,' Name:',dynamicChart.series[index].name);
  }
  //});
            
}
      
      //  This is where the chart is generated.
$(document).ready(function() 
{
 //Add Channel Load Menu
 var menu=document.getElementById("Channel Select");
 for (var channelIndex=0; channelIndex<channelKeys.length; channelIndex++)  // iterate through each channel
 {
   window.console && console.log('Name',channelKeys[channelIndex].name);
   var menuOption =new Option(channelKeys[channelIndex].name,channelIndex);
   menu.options.add(menuOption,channelIndex);
 }
 var last_date; // variable for the last date added to the chart
 window.console && console.log('Testing console');
 //make series numbers for each field
 var seriesCounter=0
 for (var channelIndex=0; channelIndex<channelKeys.length; channelIndex++)  // iterate through each channel
  {
    for (var fieldIndex=0; fieldIndex<channelKeys[channelIndex].fieldList.length; fieldIndex++)  // iterate through each channel
      {
        channelKeys[channelIndex].fieldList[fieldIndex].series = seriesCounter; 
        seriesCounter++;
      }
  }
 //make calls to load data from each channel into channelKeys array now
 // draw the chart when all the data arrives, later asyncronously add history
 for (var channelIndex=0; channelIndex<channelKeys.length; channelIndex++)  // iterate through each channel
  {
    channelKeys[channelIndex].loaded = false;  
    loadThingSpeakChannel(channelIndex,channelKeys[channelIndex].channelNumber,channelKeys[channelIndex].key,channelKeys[channelIndex].fieldList);
    
  }
 //window.console && console.log('Channel Keys',channelKeys);


 
 // load the most recent 2500 points (fast initial load) from a ThingSpeak channel into a data[] array and return the data[] array
 function loadThingSpeakChannel(sentChannelIndex,channelNumber,key,sentFieldList) {
   var fieldList= sentFieldList;
   var channelIndex = sentChannelIndex;
   // get the Channel data with a webservice call
 	$.getJSON('https://www.thingspeak.com/channels/'+channelNumber+'/feed.json?callback=?&amp;offset=0&amp;results=200;key='+key, function(data) 
   {
	   // if no access
	   if (data == '-1') {
       $('#chart-container').append('This channel is not public.  To embed charts, the channel must be public or a read key must be specified.');
       window.console && console.log('Thingspeak Data Loading Error');
     }
     for (var fieldIndex=0; fieldIndex<fieldList.length; fieldIndex++)  // iterate through each field
     {
       fieldList[fieldIndex].data =[];
       for (var h=0; h<data.feeds.length; h++)  // iterate through each feed (data point)
       {
         var p = []//new Highcharts.Point();
         var fieldStr = "data.feeds["+h+"].field"+fieldList[fieldIndex].field;
		  	 var v = eval(fieldStr);
 		  	p[0] = getChartDate(data.feeds[h].created_at);
	 	  	p[1] = parseFloat(v);
	 	  	// if a numerical value exists add it
	   		if (!isNaN(parseInt(v))) { fieldList[fieldIndex].data.push(p); }
       }
       fieldList[fieldIndex].name = channelKeys[channelIndex].name + ' ' + eval("data.channel.field"+fieldList[fieldIndex].field);
	   }
     window.console && console.log('getJSON field name:',fieldList[0].name);
     channelKeys[channelIndex].fieldList=fieldList;
     channelKeys[channelIndex].loaded=true;
     //Collected all temp/humidity data, now we calculate dp
     channelsLoaded++;
     window.console && console.log('channels Loaded:',channelsLoaded);
     window.console && console.log('channel index:',channelIndex);
     if (channelsLoaded==channelKeys.length){createChart();}
	 })
   .fail(function() { alert('getJSON request failed! '); });
 }
 function calcCustom() {
   var _fieldlist = {axis:'T',data:[], name:'Home DewPoint', series: 3};
   for(var dataIndex = 0; dataIndex< channelKeys[1].fieldList[0].data.length;dataIndex++) {
     var _tmp = [];
     _tmp[0] = channelKeys[1].fieldList[0].data[dataIndex][0];
     _tmp[1] = channelKeys[1].addField.chFunc(channelKeys[1].fieldList[0].data[dataIndex][1], channelKeys[1].fieldList[1].data[dataIndex][1]);
     _fieldlist.data.push(_tmp);
   }
   channelKeys[1].fieldList.push(_fieldlist);
 }
 
 // create the chart when all data is loaded
 function createChart() {

  //WEEMIN: calculate DewPoint
  calcCustom();
	// specify the chart options
	var chartOptions = {
	  chart: 
    {
		  renderTo: 'chart-container',
      zoomType:'y',
			events: 
      {
        load: function() 
        {
            // If the update checkbox is checked, get latest data every 15 seconds and add it to the chart
	  setInterval(function() 
            {
             if (document.getElementById("Update").checked)
             {
             loadWeather();
              for (var channelIndex=0; channelIndex<channelKeys.length; channelIndex++)  // iterate through each channel
              {  
               (function(channelIndex)
               {
                // get the data with a webservice call
                $.getJSON('https://www.thingspeak.com/channels/'+channelKeys[channelIndex].channelNumber+'/feed/last.json?callback=?&amp;offset=0&amp;location=false;key='+channelKeys[channelIndex].key, function(data) 
                { 
                  var temp_val;
                  for (var fieldIndex=0; fieldIndex<channelKeys[channelIndex].fieldList.length; fieldIndex++)
                  {
                    // if data exists
                    var fieldStr = "data.field"+channelKeys[channelIndex].fieldList[fieldIndex].field;
                    var chartSeriesIndex=channelKeys[channelIndex].fieldList[fieldIndex].series;
                    if (data && eval(fieldStr)) 
                    {
                      var p = []//new Highcharts.Point();
                      var v = eval(fieldStr);
                      p[0] = getChartDate(data.created_at);
                      p[1] = parseFloat(v);
                      //WEEMIN HACK Just push temp data into temp
                      if (channelIndex == 1) {
                        if (fieldIndex == 0 )
                        tempval = p[1];
                        else if(fieldIndex == 1  ) {
                          var rel_temp = calcDp(tempval, p[1]);
                        }
                      }
                      // get the last date if possible
                      if (dynamicChart.series[chartSeriesIndex].data.length > 0) 
                      { 
                        last_date = dynamicChart.series[chartSeriesIndex].data[dynamicChart.series[chartSeriesIndex].data.length-1].x; 
                      }
                      var shift = false ; //default for shift
                      // if a numerical value exists and it is a new date, add it
                      if (!isNaN(parseInt(v)) && (p[0] != last_date)) 
                      {
                        dynamicChart.series[chartSeriesIndex].addPoint(p, true, shift);
                        if(channelIndex == 1 && fieldIndex == 1) {
                          dynamicChart.series[3].addPoint([p[0],tempval], true, shift);
                        }

                      }   
                    }
                    //window.console && console.log('channelKeys:',channelKeys);
                    //window.console && console.log('chartSeriesIndex:',chartSeriesIndex);
                    //window.console && console.log('channel index:',channelIndex);
                    //window.console && console.log('field index:',fieldIndex);
                    //window.console && console.log('update series name:',dynamicChart.series[chartSeriesName].name);
                    //window.console && console.log('channel keys name:',channelKeys[channelIndex].fieldList[fieldIndex].name);
                  }
                });
               })(channelIndex);
              }
             }
						}, 15000);
				}
			}
		},
		rangeSelector: {
			buttons: [{
				count: 30,
				type: 'minute',
				text: '30M'
			}, {
				count: 12,
				type: 'hour',
				text: '12H'
      }, {
				count: 1,
				type: 'day',
				text: 'D'
      }, {
				count: 1,
				type: 'week',
				text: 'W'
      }, {
				count: 1,
				type: 'month',
				text: 'M'
      }, {
				count: 1,
				type: 'year',
				text: 'Y'
			}, {
				type: 'all',
				text: 'All'
			}],
			inputEnabled: true,
			selected: 1
		},
    title: {
			text: 'Home Temperature'
		},
		plotOptions: {
                   
		  line: {

        //gapSize:5
				//color: '#d62020'
				//  },
				//  bar: {
				//color: '#d52020'
				//  },
				//  column: {
			},
			series: {
//			  marker: {
//          enabled: true,
//				  radius: 2
//				},
				animation: false,
				borderWidth: 0
			}
		},
    tooltip: {
      valueDecimals: 1,
      valueSuffix: '°F',
      xDateFormat:'%Y-%m-%d<br/>%H:%M:%S %p'
			// reformat the tooltips so that local times are displayed
			//formatter: function() {
      //var d = new Date(this.x + (myOffset*60000));
      //var n = (this.point.name === undefined) ? '' : '<br/>' + this.point.name;
      //return this.series.name + ':<b>' + this.y + '</b>' + n + '<br/>' + d.toDateString() + '<br/>' + d.toTimeString().replace(/\(.*\)/, "");
			//}
    },
		xAxis: {
		  type: 'datetime',
      ordinal:false,
      min: Date.UTC(2016,03,02),
			dateTimeLabelFormats : {
        hour: '%l %p',
        minute: '%l:%M %p'
      },
      title: {
        text: 'test'
			}
		},
		yAxis: [{
            title: {
                text: 'Temperature °F'
            },
            id: 'T'
    }, {
            title: {
                text: 'Humidity %'
            },
            opposite: true,
            id: 'O'
    }],
		exporting: {
		  enabled: true,
      csv: {
        dateFormat: '%d/%m/%Y %I:%M:%S %p'
        }
		},
		legend: {
		  enabled: true
		},
    navigator: {
      baseSeries: 1,  //select which series to show in history navigator, First series is 0
      series: {
            includeInCSVExport: false
        }
		},    
    series: []
    //series: [{data:[[getChartDate("2013-06-16T00:32:40Z"),75]]}]      
	};

	// add all Channel data to the chart
  for (var channelIndex=0; channelIndex<channelKeys.length; channelIndex++)  // iterate through each channel
  {
    for (var fieldIndex=0; fieldIndex<channelKeys[channelIndex].fieldList.length; fieldIndex++)  // add each field
    {
      window.console && console.log('Channel '+channelIndex+' field '+fieldIndex);
      chartOptions.series.push({data:channelKeys[channelIndex].fieldList[fieldIndex].data,
                                index:channelKeys[channelIndex].fieldList[fieldIndex].series,
                                yAxis:channelKeys[channelIndex].fieldList[fieldIndex].axis,
                                //visible:false,
                              name: channelKeys[channelIndex].fieldList[fieldIndex].name});
    }
  }
	// set chart labels here so that decoding occurs properly
	//chartOptions.title.text = data.channel.name;
	chartOptions.xAxis.title.text = 'Date';

	// draw the chart
  dynamicChart = new Highcharts.StockChart(chartOptions);

  // update series number to account for the navigator series (The historical series at the bottom) which is the first series.
  for (var channelIndex=0; channelIndex<channelKeys.length; channelIndex++)  // iterate through each channel
  {
    for (var fieldIndex=0; fieldIndex<channelKeys[channelIndex].fieldList.length; fieldIndex++)  // and each field
    {
      for (var seriesIndex=0; seriesIndex<dynamicChart.series.length; seriesIndex++)  // compare each series name
      {
        if (dynamicChart.series[seriesIndex].name == channelKeys[channelIndex].fieldList[fieldIndex].name)
        {
          channelKeys[channelIndex].fieldList[fieldIndex].series = seriesIndex;
        }
      }
    }
  }          
  // add all history
  //dynamicChart.showLoading("Loading History..." );
  window.console && console.log('Channels: ',channelKeys.length);
  for (var channelIndex=0; channelIndex<channelKeys.length; channelIndex++)  // iterate through each channel
  {
    window.console && console.log('channelIndex: ',channelIndex);
    (function(channelIndex)
      {
        //load only 1 set of 8000 points
        loadChannelHistory(channelIndex,channelKeys[channelIndex].channelNumber,channelKeys[channelIndex].key,channelKeys[channelIndex].fieldList,0,1); 
      }
    )(channelIndex);
  }
 }
 document.getElementById('getWeather').addEventListener('click', loadWeather);

});
      
function loadOneChannel(){ 
  // load a channel selected in the popUp menu.
  var selectedChannel=document.getElementById("Channel Select");
  var maxLoads=document.getElementById("Loads").value ;
  var channelIndex = selectedChannel.selectedIndex;
  loadChannelHistory(channelIndex,channelKeys[channelIndex].channelNumber,channelKeys[channelIndex].key,channelKeys[channelIndex].fieldList,0,maxLoads); 
} 

// load next 8000 points from a ThingSpeak channel and addPoints to a series
function loadChannelHistory(sentChannelIndex,channelNumber,key,sentFieldList,sentNumLoads,maxLoads) {
   var numLoads=sentNumLoads
   var fieldList= sentFieldList;
   var channelIndex = sentChannelIndex;
   var first_Date = new Date();
   if (typeof fieldList[0].data[0] != "undefined") first_Date.setTime(fieldList[0].data[0][0]+7*60*60*1000);//adjust for 7 hour difference from GMT (Zulu time)
   else if (typeof fieldList[1].data[0] != "undefined") first_Date.setTime(fieldList[1].data[0][0]+7*60*60*1000);
   else if (typeof fieldList[2].data[0] != "undefined") first_Date.setTime(fieldList[2].data[0][0]+7*60*60*1000);
   else if (typeof fieldList[3].data[0] != "undefined") first_Date.setTime(fieldList[3].data[0][0]+7*60*60*1000);
   else if (typeof fieldList[4].data[0] != "undefined") first_Date.setTime(fieldList[4].data[0][0]+7*60*60*1000);
   else if (typeof fieldList[5].data[0] != "undefined") first_Date.setTime(fieldList[5].data[0][0]+7*60*60*1000);
   else if (typeof fieldList[6].data[0] != "undefined") first_Date.setTime(fieldList[6].data[0][0]+7*60*60*1000);
   else if (typeof fieldList[7].data[0] != "undefined") first_Date.setTime(fieldList[7].data[0][0]+7*60*60*1000);
   var end = first_Date.toJSON();
   window.console && console.log('earliest date:',end);
   window.console && console.log('sentChannelIndex:',sentChannelIndex);
   window.console && console.log('numLoads:',numLoads);
   // get the Channel data with a webservice call
 	$.getJSON('https://www.thingspeak.com/channels/'+channelNumber+'/feed.json?callback=?&amp;offset=0&amp;start=2013-01-20T00:00:00;end='+end+';key='+key, function(data) 
   {
	   // if no access
	   if (data == '-1') {
       $('#chart-container').append('This channel is not public.  To embed charts, the channel must be public or a read key must be specified.');
       window.console && console.log('Thingspeak Data Loading Error');
     }
     var tempval = []
     for (var fieldIndex=0; fieldIndex<fieldList.length; fieldIndex++)  // iterate through each field
     {
       //fieldList[fieldIndex].data =[];
       for (var h=0; h<data.feeds.length; h++)  // iterate through each feed (data point)
       {
         var p = []//new Highcharts.Point();
         var fieldStr = "data.feeds["+h+"].field"+fieldList[fieldIndex].field;
		  	 var v = eval(fieldStr);
 		  	p[0] = getChartDate(data.feeds[h].created_at);
	 	  	p[1] = parseFloat(v);
	 	  	// if a numerical value exists add it\

        //WEEMIN HACK Just push temp data into temp
        if (channelIndex == 1) {
          if (fieldIndex == 0 )
              tempval.push(p[1]);
          else if(fieldIndex == 1  ) {
            var temp = tempval.shift();
            var rel_temp = calcDp(temp, p[1]);
            fieldList[2].data.push([p[0],rel_temp]);
          }
        }
	   		if (!isNaN(parseInt(v))) { fieldList[fieldIndex].data.push(p); }
       }
       fieldList[fieldIndex].data.sort(function(a,b){return a[0]-b[0]});
       dynamicChart.series[fieldList[fieldIndex].series].setData(fieldList[fieldIndex].data,false);
       //dynamicChart.series[fieldList[fieldIndex].series].addPoint(fieldList[fieldIndex].data,false);
       //fieldList[fieldIndex].name = eval("data.channel.field"+fieldList[fieldIndex].field);
       //window.console && console.log('data added to series:',fieldList[fieldIndex].series,fieldList[fieldIndex].data);
	   }
     channelKeys[channelIndex].fieldList=fieldList;
     dynamicChart.redraw()
     window.console && console.log('channel index:',channelIndex);
     numLoads++;
     if (numLoads<maxLoads) {loadChannelHistory(channelIndex,channelNumber,key,fieldList,numLoads,maxLoads);}
	 });
}


