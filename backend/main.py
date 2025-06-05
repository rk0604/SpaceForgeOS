from flask import Flask, request, jsonify
from app.model_runner import predict

app = Flask(__name__)

@app.route('/predict', methods=['POST'])
def run_model():
    input_data = request.json
    output = predict(input_data)
    return jsonify(output)

if __name__ == "__main__":
    app.run(debug=True)
