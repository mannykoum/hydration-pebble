(function() {
  var DEFAULTS = {
    goalMl: 2800,
    unit: 'ml',
    amounts: [250, 500, 750, 1000, 237, -237]
  };
  var LOG_STORAGE_KEY = 'hydration_intake_log';
  var MAX_LOG_ENTRIES = 500;

  var UNIT_TO_ML = {
    ml: 1,
    cups: 240,
    pints: 473
  };

  function loadSettings() {
    try {
      var parsed = JSON.parse(localStorage.getItem('hydration_settings') || '{}');
      var amounts = DEFAULTS.amounts;
      if (Array.isArray(parsed.amounts)) {
        if (parsed.amounts.length === 6) {
          amounts = parsed.amounts;
        } else if (parsed.amounts.length === 4) {
          // Migrate: append new defaults
          amounts = parsed.amounts.concat([237, -237]);
        }
      }
      return {
        goalMl: Number(parsed.goalMl) || DEFAULTS.goalMl,
        unit: UNIT_TO_ML[parsed.unit] ? parsed.unit : DEFAULTS.unit,
        amounts: amounts
      };
    } catch (e) {
      return DEFAULTS;
    }
  }

  function storeSettings(settings) {
    localStorage.setItem('hydration_settings', JSON.stringify(settings));
  }

  function loadIntakeLog() {
    try {
      var raw = localStorage.getItem(LOG_STORAGE_KEY);
      var parsed = JSON.parse(raw || '[]');
      return Array.isArray(parsed) ? parsed : [];
    } catch (e) {
      return [];
    }
  }

  function storeIntakeLog(entries) {
    localStorage.setItem(LOG_STORAGE_KEY, JSON.stringify(entries));
  }

  function appendIntakeLog(deltaMl, totalMl, minute) {
    var entries = loadIntakeLog();
    entries.push({
      timestamp: Date.now(),
      minuteOfDay: Number(minute || 0),
      deltaMl: Number(deltaMl || 0),
      totalMl: Number(totalMl || 0)
    });
    if (entries.length > MAX_LOG_ENTRIES) {
      entries = entries.slice(entries.length - MAX_LOG_ENTRIES);
    }
    storeIntakeLog(entries);
  }

  function convertToMl(value, unit) {
    return Math.round(Number(value || 0) * UNIT_TO_ML[unit]);
  }

  function buildConfigUrl(settings) {
    var html = '' +
      '<!doctype html><html><head><meta name="viewport" content="width=device-width, initial-scale=1">' +
      '<style>body{font-family:sans-serif;padding:12px;}label{display:block;margin:8px 0;}input,select{width:100%;padding:8px;box-sizing:border-box;}button{width:49%;padding:10px;margin-top:12px;}small{color:#555;}</style>' +
      '</head><body><h3>Hydration Settings</h3>' +
      '<label>Goal (' + settings.unit + ')<input id="goal" type="number" step="0.1" value="' + (settings.goalMl / UNIT_TO_ML[settings.unit]).toFixed(1).replace(/\.0$/, '') + '"></label>' +
      '<label>Unit<select id="unit"><option value="ml">ml</option><option value="cups">cups</option><option value="pints">pints</option></select></label>' +
      '<small>Set 6 signed amounts (+ adds, - removes)</small>' +
      '<label>Amount 1<input id="a0" type="number" step="0.1" value="' + (settings.amounts[0] / UNIT_TO_ML[settings.unit]).toFixed(1).replace(/\.0$/, '') + '"></label>' +
      '<label>Amount 2<input id="a1" type="number" step="0.1" value="' + (settings.amounts[1] / UNIT_TO_ML[settings.unit]).toFixed(1).replace(/\.0$/, '') + '"></label>' +
      '<label>Amount 3<input id="a2" type="number" step="0.1" value="' + (settings.amounts[2] / UNIT_TO_ML[settings.unit]).toFixed(1).replace(/\.0$/, '') + '"></label>' +
      '<label>Amount 4<input id="a3" type="number" step="0.1" value="' + (settings.amounts[3] / UNIT_TO_ML[settings.unit]).toFixed(1).replace(/\.0$/, '') + '"></label>' +
      '<label>Amount 5<input id="a4" type="number" step="0.1" value="' + (settings.amounts[4] / UNIT_TO_ML[settings.unit]).toFixed(1).replace(/\.0$/, '') + '"></label>' +
      '<label>Amount 6<input id="a5" type="number" step="0.1" value="' + (settings.amounts[5] / UNIT_TO_ML[settings.unit]).toFixed(1).replace(/\.0$/, '') + '"></label>' +
      '<button onclick="save()">Save</button><button onclick="location.href=\'pebblejs://close#\'">Cancel</button>' +
      '<script>var unitSel=document.getElementById(\'unit\');unitSel.value=\'' + settings.unit + '\';' +
      'function save(){var u=unitSel.value;var payload={goal:document.getElementById(\'goal\').value,unit:u,a0:document.getElementById(\'a0\').value,a1:document.getElementById(\'a1\').value,a2:document.getElementById(\'a2\').value,a3:document.getElementById(\'a3\').value,a4:document.getElementById(\'a4\').value,a5:document.getElementById(\'a5\').value};location.href=\'pebblejs://close#\'+encodeURIComponent(JSON.stringify(payload));}</script>' +
      '</body></html>';

    return 'data:text/html;charset=utf-8,' + encodeURIComponent(html);
  }

  Pebble.addEventListener('showConfiguration', function() {
    Pebble.openURL(buildConfigUrl(loadSettings()));
  });

  Pebble.addEventListener('webviewclosed', function(e) {
    if (!e.response) {
      return;
    }

    var payload;
    try {
      payload = JSON.parse(decodeURIComponent(e.response));
    } catch (err) {
      return;
    }

    var unit = UNIT_TO_ML[payload.unit] ? payload.unit : 'ml';
    var next = {
      goalMl: Math.max(0, convertToMl(payload.goal, unit)),
      unit: unit,
      amounts: [
        convertToMl(payload.a0, unit),
        convertToMl(payload.a1, unit),
        convertToMl(payload.a2, unit),
        convertToMl(payload.a3, unit),
        convertToMl(payload.a4, unit),
        convertToMl(payload.a5, unit)
      ]
    };

    storeSettings(next);

    Pebble.sendAppMessage({
      KEY_GOAL_ML: next.goalMl,
      KEY_UNIT: unit === 'ml' ? 0 : (unit === 'cups' ? 1 : 2),
      KEY_AMOUNT_0: next.amounts[0],
      KEY_AMOUNT_1: next.amounts[1],
      KEY_AMOUNT_2: next.amounts[2],
      KEY_AMOUNT_3: next.amounts[3],
      KEY_AMOUNT_4: next.amounts[4],
      KEY_AMOUNT_5: next.amounts[5]
    });
  });

  Pebble.addEventListener('appmessage', function(e) {
    var payload = e && e.payload ? e.payload : {};
    if (
      payload.KEY_LOG_DELTA_ML === undefined ||
      payload.KEY_LOG_TOTAL_ML === undefined ||
      payload.KEY_LOG_MINUTE === undefined
    ) {
      return;
    }
    appendIntakeLog(payload.KEY_LOG_DELTA_ML, payload.KEY_LOG_TOTAL_ML, payload.KEY_LOG_MINUTE);
  });
})();
