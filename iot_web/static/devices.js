// devices.static

function editRule(rule_id) {
    window.location.href = `/device-edit-rule/${rule_id}`;
}

function deleteRule(rule_id) {
    fetch(`/device-delete-rule/${rule_id}`, { method: "POST" })
        .then(() => refreshDevicesSettings())
        .catch(error => {
            console.error("Error deleting rule:", error);
        });
}

function addRule(device_id) {
    window.location.href = `/device-add-rule/${device_id}`;
}

function saveMatrixCount(deviceId) {
            const matrixCountInput = document.getElementById(`matrixCount_${deviceId}`);
            const matrixCount = matrixCountInput.value;

            fetch(`/update-matrix-count/${deviceId}/${matrixCount}`, {
                method: 'GET'
            })
            .then(response => {
                if (response.ok) {
                    alert('Matrix count saved successfully!');
                } else {
                    alert('Error saving matrix count!');
                }
            })
            .catch(error => {
                console.error('Error saving matrix count:', error);
                alert('Error saving matrix count!');
            });
        }

function refreshDevicesSettings() {
    fetch("/devices-settings")
        .then(response => {
            if (!response.ok) {
                throw new Error("Failed to fetch devices settings");
            }
            return response.json();
        })
        .then(data => {
            const container = document.querySelector("#devices-settings-container");
            container.innerHTML = "";

            for (const [device_id, settings] of Object.entries(data)) {
                const table = document.createElement("table");
                table.innerHTML = `
                    <caption>Device ID: ${device_id}       Maxtrix count <input type="number" id="matrixCount_${ device_id }" value="${ settings.matrix_count }"><button onclick="saveMatrixCount('${ device_id }')">Save</button></caption>
                    
                    <thead>
                        <tr>
                            <th>Min</th>
                            <th>Max</th>
                            <th>Speed</th>
                            <th>Text</th>
                            <th>Actions</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${settings.rule.map(setting => `
                            <tr>
                                <td>${setting.min}</td>
                                <td>${setting.max}</td>
                                <td>${setting.speed}</td>
                                <td>${setting.text}</td>
                                <td>
                                    <button onclick="editRule(${setting.id})">Edit</button>
                                    <button onclick="deleteRule(${setting.id})">Delete</button>
                                </td>
                            </tr>
                        `).join("")}
                    </tbody>
                `;

                const addButton = document.createElement("button");
                addButton.innerText = "Добавить";
                addButton.className = "add-device-btn";
                addButton.onclick = function() {
                    addRule(device_id);
                };

                const caaasna = document.createElement("figure");
                caaasna.class = "highcharts-figure";

                const asdasd = document.createElement("div");
                asdasd.id = "container";

                caaasna.appendChild(asdasd);

                ;
                container.appendChild(table);
                container.appendChild(addButton);
                container.appendChild(caaasna)


                fetch('/temperatures')
            .then(response => response.json())
            .then(dataa => {

                const transformedData = dataa.map(obj => [obj.time, obj.data]);
                Highcharts.chart('container', {
        chart: {
            zoomType: 'x'
        },
        title: {
            text: 'Temperature ',
            align: 'left'
        },
        subtitle: {
            text: document.ontouchstart === undefined ?
                'Click and drag in the plot area to zoom in' : 'Pinch the chart to zoom in',
            align: 'left'
        },
        xAxis: {
            type: 'datetime'
        },
        yAxis: {
            title: {
                text: 'Temperature'
            }
        },
        legend: {
            enabled: false
        },
        plotOptions: {
            area: {
                fillColor: {
                    linearGradient: {
                        x1: 0,
                        y1: 0,
                        x2: 0,
                        y2: 1
                    },
                    stops: [
                        [0, Highcharts.getOptions().colors[0]],
                        [1, Highcharts.color(Highcharts.getOptions().colors[0]).setOpacity(0).get('rgba')]
                    ]
                },
                marker: {
                    radius: 2
                },
                lineWidth: 1,
                states: {
                    hover: {
                        lineWidth: 1
                    }
                },
                threshold: null
            }
        },

        series: [{
            type: 'area',
            name: 'Temperature',
            data: transformedData
        }]
    });

            })
    .catch(error => console.error(error));





            }
        })
        .catch(error => {
            console.error("Error fetching devices settings:", error);
        });
}

document.addEventListener("DOMContentLoaded", function() {
    // Обновляем список при загрузке страницы
    refreshDevicesSettings();
});
