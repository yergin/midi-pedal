let midi = null;
let midiInput = null;
let midiOutput = null;

function statusText() {
  if (!midi) {
    return "MIDI not supported";
  }
  if (!midiInput || !midiOutput) {
    return "Device not found"
  }
  return "Listening"
}

function updateStatus() {
  $("#status").text(statusText());
}

function onMIDIMessage(event) {
  let status = event.data[0]
  let data = ''
  for (var i = 1; i < event.data.length; i++) {
    data += '0x' + event.data[i].toString(16) + ' ';
  }

  $('#monitor').find('tbody').append('<tr><td>' + status.toString() + '</td><td>' + data + '</td></tr>');
}

function findInput(midiAccess) {
  for (let entry of midiAccess.inputs) {
    let input = entry[1]
    if (input.name == 'Footsy') {
      return input
    }
  }
  return null
}

function findOutput(midiAccess) {
  for (let entry of midiAccess.outputs) {
    let output = entry[1]
    if (output.name == 'Footsy') {
      return output
    }
  }
  return null
}

function findDevice(midiAccess) {
  let input = findInput(midiAccess)
  if (input) {
    if (!midiInput || input.id != midiInput.id) {
      input.onmidimessage = onMIDIMessage
    }
  }
  midiInput = input

  let output = findOutput(midiAccess)
  if (output) {
    if (!midiOutput || output.id != midiOutput.id) {
      const data = [0xf0, 0x00, 0x50, 0x07, 0x77, 0x54, 0x7f, 0xf7]
      //const data = [0xb0, 11, 0] // controller change
      //const data = [0xc0, 0] // prog change
      output.send(data)
    }
  }
  midiOutput = output

  updateStatus()
}

function onMIDISuccess(midiAccess) {
  if (!midiAccess.sysexEnabled) {
    updateStatus()
    return
  }
  midi = midiAccess
  midiAccess.onstatechange = event => {
    findDevice(midiAccess);
  };
  findDevice(midiAccess)
}

function onMIDIFailure(msg) {
  midi = null;
  updateStatus()
  console.error("Failed to get MIDI access - " + msg)
}

navigator.requestMIDIAccess({ sysex: true }).then(onMIDISuccess, onMIDIFailure)