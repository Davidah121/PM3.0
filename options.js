
$(document).ready(function(){
	checkLoginStatus();
	$.ajax({
		type: 'GET',
		url: '/api/get_user_info',
		success: function(result) {
			$("#UsernameHeaderID").text(`Hello ${result.Username}`);
		},
		error: function(err) { 
			//redirect to login
			alert("Not signed in");
			window.location.href = "/login.html";
		}
	});
})