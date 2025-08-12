function attemptLogin()
{
	var userValue = document.getElementById("usernameID").value;
	var passwordValue = document.getElementById("passwordID").value;

	$.ajax({
		type: 'POST',
		url: '/api/login',
		data: `{"Username": "${userValue}", "Password": "${passwordValue}"}`,
		contentType: 'application/json',
		dataType: 'json',
		success: function(result) { 
			if(result["status"] == "Success")
			{
				//redirect to index
				window.location.href = "/";
			}
			else
			{
				alert(result["status"]);
			}
		},
		error: function(err) { console.log(err); }
	});
}

function attemptCreateAccount()
{
	var userValue = document.getElementById("usernameID").value;
	var passwordValue = document.getElementById("passwordID").value;

	$.ajax({
		type: 'POST',
		url: '/api/create_account',
		data: `{"Username": "${userValue}", "Password": "${passwordValue}"}`,
		contentType: 'application/json',
		dataType: 'json',
		success: function(result) { 
			if(result["status"] == "Success")
			{
				//redirect to index
				attemptLogin();
			}
			else
			{
				alert(result["status"]);
			}
		},
		error: function(err) { console.log(err); }
	});
}

function attemptLogout()
{
	$.ajax({
		type: 'POST',
		url: '/api/logout',
		success: function(result) { 
			if(result["status"] == "Success")
			{
				//redirect to index
				alert("LOG OUT");
				window.location.href = "/login.html";
			}
			else
			{
				alert(result["status"]);
			}
		},
		error: function(err) { console.log(err); }
	});
}

function checkLoginStatus()
{
	$.ajax({
		type: 'GET',
		url: '/api/get_user_info',
		success: function(result) {
			//remove login button on side panel. Replace with logout button
			$("#LoginButID").text("Logout").attr("href", "javascript:void(0)").click(function(){
				attemptLogout();
			});
		},
		error: function(err) {
			//remove settings button on side panel. It'll redirect you to login page anyways.
			$("#SettingsButID").hide();
		}
	})
}

function attemptDeleteAccount()
{
	$.ajax({
		type: 'POST',
		url: '/api/delete_account',
		success: function(result) { 
			if(result["status"] == "Success")
			{
				//redirect to index
				window.location.href = "/login.html";
			}
			else
			{
				alert(result["status"]);
			}
		},
		error: function(err) { console.log(err); }
	});
}

function attemptChangePassword()
{
	var oldPass = $("#OldPasswordBox").val();
	var newPass = $("#NewPasswordBox").val();
	
	$.ajax({
		type: 'POST',
		url: '/api/change_password',
		data: `{"OldPassword": "${oldPass}", "NewPassword": "${newPass}"}`,
		contentType: 'application/json',
		dataType: 'json',
		success: function(result) {
			if(result["status"] == "Success")
			{
				alert("Successful password change");
				window.location.href = "/login.html";
			}
			else
			{
				alert(result["status"]);
			}
		},
		error: function(err) { console.log(err); }
	});
}

function callGet()
{
	var req = new XMLHttpRequest()
	req.open("GET", "/api/list_all_entries", true)
	req.onreadystatechange = function() {
		if(req.readyState == 4 && req.status == 200) {
			var json = JSON.parse(req.responseText)
			fillOutEntries(json);
		}
	}
	
	req.send()
}

function addOrEditEntry(Name, Username, Password, Description)
{
	$.ajax({
		type: 'POST',
		url: '/api/add_entry',
		data: `{"Name": "${Name}", "Username": "${Username}", "Password": "${Password}", "Description": "${Description}"}`,
		contentType: 'application/json',
		dataType: 'json',
		success: function(result) {
			if(result["status"] == "Success")
			{
				//reload all entries again.
				let topDiv = $("#AddEntryDiv");
				topDiv.empty();
				callGet();
			}
			else
			{
				alert(result["status"]);
			}
		},
		error: function(err) { 
			if(err["status"] == 401)
			{
				alert("Not signed in.");
			}
		}
	});
}

function deleteEntry(entryName)
{	
	$.ajax({
		type: 'POST',
		url: '/api/delete_entry',
		data: `{"Name": "${entryName}"}`,
		contentType: 'application/json',
		dataType: 'json',
		success: function(result) {
			if(result["status"] == "Success")
			{
				//reload all entries again.
				callGet();
			}
			else
			{
				alert(result["status"]);
			}
		},
		error: function(err) { 
			if(err["status"] == 401)
			{
				alert("Not signed in.");
			}
		}
	});
}

function fillOutEntries(inputJSON)
{
	$("#EntryContainer").empty();

	for(var i=0; i<inputJSON["Entries"].length; i++)
	{
		// inputJSON[]
		let rootEntryDiv = $("<div></div>").addClass("w3-display-container w3-round-large entry_class");
		let entryName = inputJSON["Entries"][i].Name;
		rootEntryDiv.append($("<h2></h2>").text(entryName));
		let spanButton = $("<span></span>").addClass("w3-button w3-display-topright").append(
			$("<i></i>").addClass("fa fa-arrow-down")
		);
		let usernameBox = $("<div></div>").addClass("w3-row w3-container input_css_container").append(
						$("<label></label>").addClass("w3-input w3-half").text("Username:")
					).append(
						$("<input></input>").addClass("w3-input w3-half w3-round-large").attr("type", "text").val(inputJSON["Entries"][i].Username)
					);
		let passwordBox = $("<div></div>").addClass("w3-row w3-container input_css_container").append(
						$("<label></label>").addClass("w3-input w3-half").text("Password:")
					).append(
						$("<input></input>").addClass("w3-input w3-half w3-round-large").attr("type", "text").val(inputJSON["Entries"][i].Password)
					);
		let dateCreatedLabel = $("<div></div>").addClass("w3-row w3-container input_css_container").append(
						$("<label></label>").addClass("w3-input w3-half").text("Date-Created:")
					).append(
						$("<h3></h3>").addClass("w3-half").text(inputJSON["Entries"][i]["Date-Created"])
					);
		let dateUpdatedLabel = $("<div></div>").addClass("w3-row w3-container input_css_container").append(
						$("<label></label>").addClass("w3-input w3-half").text("Date-Updated:")
					).append(
						$("<h3></h3>").addClass("w3-half").text(inputJSON["Entries"][i]["Date-Updated"])
					);
		let descriptionBox = $("<div></div>").addClass("w3-row w3-container input_css_container_noline").append(
						$("<label></label>").addClass("w3-input").text("Description:")
					).append(
						$("<textarea></textarea>").addClass("w3-input w3-round-large").attr("rows", "8").attr("cols", "50").text(inputJSON["Entries"][i].Description)
					);
		let trashButton = $("<div></div>").addClass("w3-container").append(
					$("<button></button>").addClass("w3-button w3-display-bottomright w3-red").append(
						$("<i></i>").addClass("fa fa-trash")
					)
				);
		
		let submitButton = $("<div></div>").addClass("w3-container").append(
					$("<button></button>").addClass("w3-button w3-display-bottomleft w3-green").append(
						$("<i></i>").addClass("fa fa-check")
					)
				);

		let infoContainer = $("<div></div>").addClass("w3-container w3-hide").append(
				$("<div></div>").addClass("w3-container").append(
					usernameBox
				).append(
					passwordBox
				).append(
					dateCreatedLabel
				).append(
					dateUpdatedLabel
				).append(
					descriptionBox
				)
			).append(
				trashButton
			).append(
				submitButton
			);

		spanButton.click(function(){
			infoContainer.toggleClass("w3-show");
		});

		trashButton.click(function() {
			deleteEntry(entryName);
		});
		submitButton.click(function() {
			addOrEditEntry(entryName, usernameBox.find("input").val(), passwordBox.find("input").val(), descriptionBox.find("textarea").val());
		});

		rootEntryDiv.append(spanButton);
		rootEntryDiv.append($("<div></div>").addClass("w3-container").append(infoContainer));
		//add to rootEntryDiv
		$("#EntryContainer").append(rootEntryDiv);
	}
}

function mapDropDownFunction(callerID, containerID) {
	$("#"+callerID).click(function(){
		$("#"+containerID).toggleClass("w3-show");
	});
}

function addNewEntryDialogue()
{
	//add new div in front of everything else. darken everything else around it
	let topDiv = $("#AddEntryDiv");
	topDiv.empty();

	let rootEntryDiv = $("<div></div>").addClass("w3-display-container w3-round-large entry_class");
	let idBox = $("<div></div>").addClass("w3-row w3-container input_css_container").append(
					$("<label></label>").addClass("w3-input w3-half").text("Name:")
				).append(
					$("<input></input>").addClass("w3-input w3-half w3-round-large").attr("type", "text")
				);
	let usernameBox = $("<div></div>").addClass("w3-row w3-container input_css_container").append(
					$("<label></label>").addClass("w3-input w3-half").text("Username:")
				).append(
					$("<input></input>").addClass("w3-input w3-half w3-round-large").attr("type", "text")
				);
	let passwordBox = $("<div></div>").addClass("w3-row w3-container input_css_container").append(
					$("<label></label>").addClass("w3-input w3-half").text("Password:")
				).append(
					$("<input></input>").addClass("w3-input w3-half w3-round-large").attr("type", "text")
				);
	let descriptionBox = $("<div></div>").addClass("w3-row w3-container input_css_container_noline").append(
					$("<label></label>").addClass("w3-input").text("Description:")
				).append(
					$("<textarea></textarea>").addClass("w3-input w3-round-large").attr("rows", "8").attr("cols", "50")
				);
	let trashButton = $("<div></div>").addClass("w3-container").append(
				$("<button></button>").addClass("w3-button w3-display-bottomright w3-red").append(
					$("<i></i>").addClass("fa fa-trash")
				)
			);
	let submitButton = $("<div></div>").addClass("w3-container").append(
				$("<button></button>").addClass("w3-button w3-display-bottomleft w3-green").append(
					$("<i></i>").addClass("fa fa-check")
				)
			);
	let infoContainer = $("<div></div>").addClass("w3-container").append(
			$("<div></div>").addClass("w3-container").append(
				idBox
			).append(
				usernameBox
			).append(
				passwordBox
			).append(
				descriptionBox
			)
		).append(
			trashButton
		).append(
			submitButton
		);

	trashButton.find("button").click(function(){
		topDiv.empty();
	});

	submitButton.find("button").click(function() {

		addOrEditEntry(idBox.find("input").val(), usernameBox.find("input").val(), passwordBox.find("input").val(), descriptionBox.find("textarea").val());
	});
	rootEntryDiv.append($("<div></div>").addClass("w3-container").append(infoContainer));

	//add to rootEntryDiv
	topDiv.append(rootEntryDiv);
}
function w3_open()
{
  	document.getElementById("linkSidebar").style.width = "100%";
  	document.getElementById("linkSidebar").style.display = "block";
}

function w3_close()
{
  document.getElementById("linkSidebar").style.display = "none";
}

function togglePasswordVisibility()
{
	var obj = $("#passwordID");
	if(obj.get(0).type == "text")
		obj.get(0).type = "password";
	else
		obj.get(0).type = "text";
}