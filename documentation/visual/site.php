<?php

$siteRoot = 'https://www.hexaverse.net';
$globalCurrent = '';

function PutLink($name, $url)
{
	global $globalCurrent;
	if ($globalCurrent == $name)
	{
		echo "<li class=\"menu_current\"><div class=\"menu_link\">$name</div></li>";
		return;
	}
	echo "<li class=\"menu\"><a href=\"$url\" class=\"menu_link\"><div class=\"menu_link\">$name</div></a></li>";
}

function Site($arElements, $title)
{
	global $globalCurrent,$siteRoot;
	$globalCurrent = $title;
	?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
  <head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    <title>Hexaverse - <?php echo $title; ?></title>
	<link rel="stylesheet" href="<?php echo $siteRoot;?>/visual/style.css" />
	<link rel="shortcut icon" href="<?php echo $siteRoot;?>/visual/visual.ico" />
  </head>
  <body>
	<div class="super">
		<div class="head"><img src="<?php echo $siteRoot;?>/visual/hexaverse_top.png" alt="Top Banner"/></div>
		
		<div class="body">
			<div class="menu">
			<ul class="menu">
				<?php
					PutLink("Home","/");
					PutLink("News","/pages/news.php");
					//PutLink("Forum","");
					//PutLink("Gallery","");
					//PutLink("&larr; Wiki","/gameWiki/");
					PutLink("&larr; Forum","/gameForum/");
					PutLink("&larr; Steam Greenlight","https://steamcommunity.com/sharedfiles/filedetails/?id=625864433");
					PutLink("Downloads",$siteRoot."/pages/download/");
					PutLink("Impressum",$siteRoot."/pages/impressum.php");
				?>
			</ul>
			</div>
			<div class="messages">
<?php
	foreach ($arElements as $el)
	{
		echo '<div class="message">';
			echo $el;
		echo '</div>';
	}
?>
			
			</div>
		</div>
		<div class="footer">
			<div class="footer_inner">Copyright &copy; Stefan Elsen 2016</div>
		</div>
	</div>
	
  </body>
</html>

	<?php
}










