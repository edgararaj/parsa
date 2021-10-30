function router() {
	const routes = [
		{ path: "/", html: `#include "slideshow.html"` },
		{ path: "/mais", html: `#include "meis.html"` },
	];
	let route_matched = routes.find(route => route.path == location.pathname);
	if (!route_matched) {
		route_matched = routes[0];
		history.replaceState(0, 0, route_matched.path);
	}

	const content = document.getElementById("content");
	content.innerHTML = route_matched.html;
}

router();

window.addEventListener("popstate", router);

document.body.addEventListener("click", e => {
	if (e.target.matches("[route]")) {
		e.preventDefault();
		history.pushState(0, 0, e.target.href);
		router();
	}
});
