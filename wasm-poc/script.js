function run(input, mod)
{
	var inData = mod._malloc(input.length);
	console.log(input.length);

	mod.HEAPU8.set(input, inData);
	var blob = mod._fontToJSON(inData, input.length);

	var outData = mod._blob_data(blob);
	var outLength = mod._blob_length(blob);

	var outArray = mod.HEAPU8.subarray(outData, outLength);
	console.log(outArray);

	mod._free_blob(blob);
	mod._free(inData);
}


function handleFile(input) {
	let file = input.files[0];

	console.log(`File name: ${file.name}`);

	let reader = new FileReader();
	reader.readAsArrayBuffer(file);

	reader.onload = function() {
		Module().then((mod) => {
			run(new Uint8Array(reader.result), mod);
		});
	};

	reader.onerror = function() {
		console.log(reader.error);
	};
}
